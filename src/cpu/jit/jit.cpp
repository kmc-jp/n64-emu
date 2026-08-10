#include "cpu/jit/jit.h"
#include "cpu/cached_interp.h"
#include "cpu/cpu.h"
#include "cpu/idle_skip.h"
#include "cpu/jit/helpers.h"
#include "cpu/jit/invalidate_hook.h"
#include "memory/memory_map.h"
#include "mmu/mmu.h"
#include "n64_system/interrupt.h"
#include "n64_system/machine_advance.h"
#include "n64_system/scheduler.h"
#include "utils/log.h"
#include <chrono>
#include <cstdlib>

namespace N64 {
namespace Cpu {
namespace Jit {

namespace {
// Flush RSP + scheduler after this many guest cycles of soft-chained work.
// Much cheaper than per-BB / per-delay-slot advance; still far shorter than a
// half-line (~6000), so CPU?RSP and PI/AI waits cannot starve.
constexpr int kAdvanceEveryCycles = 1024;

struct JitProf {
    bool enabled = false;
    bool times = false; // per-call chrono; expensive at ~10M blocks/s
    bool inited = false;
    double native_ms = 0;
    double fallback_ms = 0;
    double compile_ms = 0;
    double advance_ms = 0;
    double dispatch_ms = 0;
    uint64_t native_calls = 0;
    uint64_t native_cycles = 0;
    uint64_t fallback_calls = 0;
    uint64_t fallback_cycles = 0;
    uint64_t compiles = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    uint64_t tlb_slow = 0;
    uint64_t invalidates = 0;
    uint64_t advances = 0;
    uint64_t idle_warps = 0;
    uint64_t idle_cycles = 0;
    uint64_t chain_links = 0;
};

JitProf &prof() {
    static JitProf p;
    if (!p.inited) {
        p.inited = true;
        const char *e = std::getenv("N64_PROFILE_FRAME");
        const char *j = std::getenv("N64_PROFILE_JIT");
        p.enabled = (e && e[0] && e[0] != '0') || (j && j[0] && j[0] != '0');
        const char *t = std::getenv("N64_PROFILE_JIT_TIMES");
        p.times = p.enabled && t && t[0] && t[0] != '0';
    }
    return p;
}

using clock = std::chrono::steady_clock;

inline double ms_since(clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
}
} // namespace

void jit_profile_note_invalidate() {
    auto &p = prof();
    if (p.enabled)
        ++p.invalidates;
}

void jit_profile_dump() {
    auto &p = prof();
    if (!p.enabled)
        return;
    const double avg_cyc =
        p.native_calls ? double(p.native_cycles) / double(p.native_calls) : 0.0;
    if (p.times) {
        const double total = p.native_ms + p.fallback_ms + p.compile_ms +
                             p.advance_ms + p.dispatch_ms;
        const double inv = total > 0 ? 100.0 / total : 0.0;
        Utils::info(
            "jit profile (1s): native={:.2f}ms({:.0f}%) fallback={:.2f}ms({:.0f}%) "
            "compile={:.2f}ms({:.0f}%) advance={:.2f}ms({:.0f}%) "
            "dispatch={:.2f}ms({:.0f}%) | calls native={} fb={} compile={} "
            "cache hit/miss={}/{} tlb_slow={} inval={} adv={} idle={}/{}c "
            "chain={} | "
            "cyc native={} fb={} avg_blk={:.1f}",
            p.native_ms, p.native_ms * inv, p.fallback_ms, p.fallback_ms * inv,
            p.compile_ms, p.compile_ms * inv, p.advance_ms, p.advance_ms * inv,
            p.dispatch_ms, p.dispatch_ms * inv, p.native_calls, p.fallback_calls,
            p.compiles, p.cache_hits, p.cache_misses, p.tlb_slow, p.invalidates,
            p.advances, p.idle_warps, p.idle_cycles, p.chain_links,
            p.native_cycles, p.fallback_cycles, avg_cyc);
    } else {
        Utils::info(
            "jit profile (1s): calls native={} fb={} compile={} "
            "cache hit/miss={}/{} tlb_slow={} inval={} adv={} idle={}/{}c "
            "chain={} | "
            "cyc native={} fb={} avg_blk={:.1f}",
            p.native_calls, p.fallback_calls, p.compiles, p.cache_hits,
            p.cache_misses, p.tlb_slow, p.invalidates, p.advances, p.idle_warps,
            p.idle_cycles, p.chain_links, p.native_cycles, p.fallback_cycles,
            avg_cyc);
    }
    p.native_ms = p.fallback_ms = p.compile_ms = p.advance_ms = p.dispatch_ms =
        0;
    p.native_calls = p.native_cycles = 0;
    p.fallback_calls = p.fallback_cycles = 0;
    p.compiles = p.cache_hits = p.cache_misses = 0;
    p.tlb_slow = p.invalidates = p.advances = 0;
    p.idle_warps = p.idle_cycles = 0;
    p.chain_links = 0;
}

Dynarec Dynarec::instance_{};

Dynarec &Dynarec::get_instance() { return instance_; }

void Dynarec::reset() {
    cache_.clear();
    CachedInterp::clear();
    set_code_invalidate_hook([](uint32_t paddr, uint32_t length) {
        g_dynarec().invalidate_range(paddr, length);
        CachedInterp::invalidate_range(paddr, length);
    });
}

void Dynarec::invalidate_page(uint32_t paddr) {
    if (!cache_.page_has_code(paddr))
        return;
    jit_profile_note_invalidate();
    cache_.invalidate_page(paddr);
}

void Dynarec::invalidate_range(uint32_t paddr, uint32_t length) {
    // Cheap reject: single-page data writes (framebuffer) dominate.
    if (length <= 8 && !cache_.page_has_code(paddr))
        return;
    if (length > 8) {
        bool any = false;
        const uint32_t start = paddr & ~0xFFFu;
        const uint64_t end64 =
            static_cast<uint64_t>(paddr) + static_cast<uint64_t>(length) - 1;
        const uint32_t end =
            end64 > 0xffffffffu ? 0xffffffffu : static_cast<uint32_t>(end64);
        for (uint64_t p = start; p <= end; p += 0x1000u) {
            if (cache_.page_has_code(static_cast<uint32_t>(p))) {
                any = true;
                break;
            }
        }
        if (!any)
            return;
    }
    jit_profile_note_invalidate();
    cache_.invalidate_range(paddr, length);
}

void invalidate_code_page(uint32_t paddr) {
    g_dynarec().invalidate_page(paddr);
}

void invalidate_code_range(uint32_t paddr, uint32_t length) {
    g_dynarec().invalidate_range(paddr, length);
}

int Dynarec::run_interpreter_fallback() {
    auto &p = prof();
    if (!p.enabled) {
        g_cpu().step();
        return static_cast<int>(CPU_CYCLES_PER_INST);
    }
    if (p.times) {
        const auto t0 = clock::now();
        g_cpu().step();
        p.fallback_ms += ms_since(t0);
    } else {
        g_cpu().step();
    }
    const int got = static_cast<int>(CPU_CYCLES_PER_INST);
    ++p.fallback_calls;
    p.fallback_cycles += static_cast<uint64_t>(got);
    return got;
}

static bool should_interpret_paddr(uint32_t paddr) {
    // IPL3 / boot code in SP DMEM is sensitive; keep it on the interpreter
    // until the dynarec is proven correct there.
    return PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END;
}

CompiledBlock *Dynarec::compile(uint32_t vaddr, uint32_t paddr) {
    auto &p = prof();
    if (!p.enabled) {
        IrBlock ir;
        if (!translate_block(vaddr, paddr, ir))
            return nullptr;
        BlockFn fn = emit_block(ir, cache_);
        cache_.insert(paddr, fn, static_cast<uint16_t>(ir.ops.size()));
        return cache_.lookup(paddr);
    }
    CompiledBlock *block = nullptr;
    if (p.times) {
        const auto t0 = clock::now();
        IrBlock ir;
        if (!translate_block(vaddr, paddr, ir))
            return nullptr;
        BlockFn fn = emit_block(ir, cache_);
        cache_.insert(paddr, fn, static_cast<uint16_t>(ir.ops.size()));
        block = cache_.lookup(paddr);
        p.compile_ms += ms_since(t0);
    } else {
        IrBlock ir;
        if (!translate_block(vaddr, paddr, ir))
            return nullptr;
        BlockFn fn = emit_block(ir, cache_);
        cache_.insert(paddr, fn, static_cast<uint16_t>(ir.ops.size()));
        block = cache_.lookup(paddr);
    }
    ++p.compiles;
    return block;
}

int Dynarec::run(int budget) {
    if (budget < 1)
        budget = 1;

    auto &cpu = g_cpu();
    ExecState *exec = exec_state_ptr();
    auto &p = prof();
    const bool prof_on = p.enabled;
    const bool prof_times = p.times;
    int total = 0;
    int pending = 0;

    const auto flush_pending = [&]() {
        if (pending < 1)
            return;
        if (prof_on) {
            if (prof_times) {
                const auto t0 = clock::now();
                N64System::advance_after_cpu(pending);
                p.advance_ms += ms_since(t0);
            } else {
                N64System::advance_after_cpu(pending);
            }
            ++p.advances;
        } else {
            N64System::advance_after_cpu(pending);
        }
        pending = 0;
    };

    const auto apply_idle_if_pending = [&]() {
        if (!idle_skip_pending())
            return;
        // Soft-chain COUNT is ahead of the scheduler; catch up before warping.
        flush_pending();
        const int skipped = idle_skip_apply_pending();
        if (skipped > 0) {
            total += skipped;
            if (prof_on) {
                ++p.idle_warps;
                p.idle_cycles += static_cast<uint64_t>(skipped);
            }
        }
    };

    const auto credit = [&](int got) {
        total += got;
        pending += got;
        idle_skip_consume(got);
        if (pending >= kAdvanceEveryCycles)
            flush_pending();
        apply_idle_if_pending();
    };

    idle_skip_begin_slice(budget);

    // Soft-chain within the half-line budget. Batch RSP + scheduler every
    // kAdvanceEveryCycles (and on overdue events / abort / exit).
    while (total < budget) {
        const auto loop_t0 = prof_times ? clock::now() : clock::time_point{};

        if (cpu.delay_slot) {
            if (exec->annul_delay_slot) {
                cpu.delay_slot = false;
                exec->annul_delay_slot = false;
                continue;
            }
            credit(run_interpreter_fallback());
            continue;
        }

        uint64_t until = g_scheduler().cycles_until_next_event();
        if (until != UINT64_MAX && until <= static_cast<uint64_t>(pending)) {
            flush_pending();
            until = g_scheduler().cycles_until_next_event();
        } else if (until != UINT64_MAX) {
            until -= static_cast<uint64_t>(pending);
        }

        if (until == 0) {
            flush_pending();
            if (prof_on) {
                if (prof_times) {
                    const auto t0 = clock::now();
                    N64System::advance_after_cpu(0);
                    p.advance_ms += ms_since(t0);
                } else {
                    N64System::advance_after_cpu(0);
                }
                ++p.advances;
            } else {
                N64System::advance_after_cpu(0);
            }
            if (g_scheduler().cycles_until_next_event() == 0)
                credit(run_interpreter_fallback());
            continue;
        }

        int slice = budget - total;
        if (until < static_cast<uint64_t>(slice))
            slice = static_cast<int>(until);
        if (slice < 1) {
            flush_pending();
            break;
        }

        const uint32_t pc32 = static_cast<uint32_t>(cpu.get_pc64());
        uint32_t paddr;
        if (auto direct = Mmu::try_direct_map(pc32)) {
            paddr = *direct;
        } else {
            auto resolved = Mmu::resolve_vaddr_slow(pc32);
            if (!resolved.has_value()) {
                credit(run_interpreter_fallback());
                continue;
            }
            if (prof_on)
                ++p.tlb_slow;
            paddr = *resolved;
        }

        if (should_interpret_paddr(paddr)) {
            credit(run_interpreter_fallback());
            continue;
        }

        // Match Cpu::step: retire delay-slot flags before interrupt check.
        cpu.prev_delay_slot = cpu.delay_slot;
        cpu.delay_slot = false;

        if (cpu.should_service_interrupt()) {
            cpu.handle_exception(ExceptionCode::INTERRUPT, 0, false);
            credit(static_cast<int>(CPU_CYCLES_PER_INST));
            // Delivering an interrupt: publish pending machine time first.
            flush_pending();
            continue;
        }

        CompiledBlock *block = cache_.lookup(paddr);
        if (!block) {
            if (prof_on)
                ++p.cache_misses;
            block = compile(pc32, paddr);
            if (!block) {
                credit(run_interpreter_fallback());
                continue;
            }
        } else if (prof_on) {
            ++p.cache_hits;
        }

        // Intentionally allow a block to run slightly past `until` / slice.
        // Clamping to interpreter or returning to the outer loop here was the
        // main soft-chain slowdown when PI/AI timers are frequent.

        if (prof_times)
            p.dispatch_ms += ms_since(loop_t0);

        // Block linking: re-enter compiled code for the next PC without the
        // full outer dispatcher (scheduler/TLB/compile) when possible.
        for (;;) {
            exec->aborted = false;
            exec->annul_delay_slot = false;
            int got;
            if (prof_on) {
                if (prof_times) {
                    const auto t0 = clock::now();
                    const int taken = block->fn();
                    p.native_ms += ms_since(t0);
                    got = taken > 0 ? taken : 1;
                } else {
                    const int taken = block->fn();
                    got = taken > 0 ? taken : 1;
                }
                ++p.native_calls;
                p.native_cycles += static_cast<uint64_t>(got);
            } else {
                const int taken = block->fn();
                got = taken > 0 ? taken : 1;
            }

            const int total_before = total;
            credit(got);
            if (exec->aborted) {
                flush_pending();
                break; // leave chain + outer; final flush below
            }
            if (total >= budget)
                break;
            // Idle warp (or any extra credit) ? re-check events in outer loop.
            if (total > total_before + got)
                break;
            if (cpu.delay_slot)
                break;

            uint64_t until_next = g_scheduler().cycles_until_next_event();
            if (until_next != UINT64_MAX &&
                until_next <= static_cast<uint64_t>(pending))
                break;

            cpu.prev_delay_slot = cpu.delay_slot;
            cpu.delay_slot = false;
            if (cpu.should_service_interrupt()) {
                cpu.handle_exception(ExceptionCode::INTERRUPT, 0, false);
                credit(static_cast<int>(CPU_CYCLES_PER_INST));
                flush_pending();
                break;
            }

            const uint32_t next_pc = static_cast<uint32_t>(cpu.get_pc64());
            uint32_t next_paddr;
            if (auto direct = Mmu::try_direct_map(next_pc)) {
                next_paddr = *direct;
            } else {
                auto resolved = Mmu::resolve_vaddr_slow(next_pc);
                if (!resolved.has_value())
                    break;
                if (prof_on)
                    ++p.tlb_slow;
                next_paddr = *resolved;
            }
            if (should_interpret_paddr(next_paddr))
                break;

            CompiledBlock *next = cache_.lookup(next_paddr);
            if (!next)
                break;
            if (prof_on) {
                ++p.cache_hits;
                ++p.chain_links;
            }
            block = next;
        }
        if (exec->aborted)
            break;
    }

    apply_idle_if_pending();
    flush_pending();

    if (total < 1) {
        const int got = run_interpreter_fallback();
        if (prof_on) {
            if (prof_times) {
                const auto t0 = clock::now();
                N64System::advance_after_cpu(got);
                p.advance_ms += ms_since(t0);
            } else {
                N64System::advance_after_cpu(got);
            }
            ++p.advances;
        } else {
            N64System::advance_after_cpu(got);
        }
        return got;
    }
    return total;
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

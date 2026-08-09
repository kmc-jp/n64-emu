#include "cpu/jit/jit.h"
#include "cpu/cached_interp.h"
#include "cpu/cpu.h"
#include "cpu/jit/helpers.h"
#include "cpu/jit/invalidate_hook.h"
#include "memory/memory_map.h"
#include "mmu/mmu.h"
#include "n64_system/interrupt.h"
#include "n64_system/machine_advance.h"
#include "n64_system/scheduler.h"
#include "utils/log.h"

namespace N64 {
namespace Cpu {
namespace Jit {

namespace {
// Flush RSP + scheduler after this many guest cycles of soft-chained work.
// Much cheaper than per-BB / per-delay-slot advance; still far shorter than a
// half-line (~6000), so CPU↔RSP and PI/AI waits cannot starve.
constexpr int kAdvanceEveryCycles = 1024;
} // namespace

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

void Dynarec::invalidate_page(uint32_t paddr) { cache_.invalidate_page(paddr); }

void Dynarec::invalidate_range(uint32_t paddr, uint32_t length) {
    cache_.invalidate_range(paddr, length);
}

void invalidate_code_page(uint32_t paddr) {
    g_dynarec().invalidate_page(paddr);
}

void invalidate_code_range(uint32_t paddr, uint32_t length) {
    g_dynarec().invalidate_range(paddr, length);
}

int Dynarec::run_interpreter_fallback() {
    g_cpu().step();
    return static_cast<int>(CPU_CYCLES_PER_INST);
}

static bool should_interpret_paddr(uint32_t paddr) {
    // IPL3 / boot code in SP DMEM is sensitive; keep it on the interpreter
    // until the dynarec is proven correct there.
    return PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END;
}

CompiledBlock *Dynarec::compile(uint32_t vaddr, uint32_t paddr) {
    IrBlock ir;
    if (!translate_block(vaddr, paddr, ir))
        return nullptr;
    BlockFn fn = emit_block(ir, cache_);
    cache_.insert(paddr, fn, static_cast<uint16_t>(ir.ops.size()));
    return cache_.lookup(paddr);
}

int Dynarec::run(int budget, bool rsp_thread) {
    if (budget < 1)
        budget = 1;

    auto &cpu = g_cpu();
    ExecState *exec = exec_state_ptr();
    int total = 0;
    int pending = 0;

    const auto flush_pending = [&]() {
        if (pending < 1)
            return;
        N64System::advance_after_cpu(pending, rsp_thread);
        pending = 0;
    };

    const auto credit = [&](int got) {
        total += got;
        pending += got;
        if (pending >= kAdvanceEveryCycles)
            flush_pending();
    };

    // Soft-chain within the half-line budget. Batch RSP + scheduler every
    // kAdvanceEveryCycles (and on overdue events / abort / exit).
    while (total < budget) {
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
            N64System::advance_after_cpu(0, rsp_thread);
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
            block = compile(pc32, paddr);
            if (!block) {
                credit(run_interpreter_fallback());
                continue;
            }
        }

        // Intentionally allow a block to run slightly past `until` / slice.
        // Clamping to interpreter or returning to the outer loop here was the
        // main soft-chain slowdown when PI/AI timers are frequent.

        exec->aborted = false;
        exec->annul_delay_slot = false;
        const int taken = block->fn();
        const int got = taken > 0 ? taken : 1;
        credit(got);
        if (exec->aborted) {
            flush_pending();
            break;
        }
    }

    flush_pending();

    if (total < 1) {
        const int got = run_interpreter_fallback();
        N64System::advance_after_cpu(got, rsp_thread);
        return got;
    }
    return total;
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

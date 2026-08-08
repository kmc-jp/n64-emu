#include "cpu/jit/jit.h"
#include "cpu/cpu.h"
#include "cpu/jit/helpers.h"
#include "cpu/jit/invalidate_hook.h"
#include "memory/memory_map.h"
#include "mmu/mmu.h"
#include "n64_system/interrupt.h"
#include "utils/log.h"

namespace N64 {
namespace Cpu {
namespace Jit {

Dynarec Dynarec::instance_{};

Dynarec &Dynarec::get_instance() { return instance_; }

void Dynarec::reset() {
    cache_.clear();
    set_code_invalidate_hook([](uint32_t paddr, uint32_t length) {
        g_dynarec().invalidate_range(paddr, length);
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

int Dynarec::run(int budget) {
    if (budget < 1)
        budget = 1;

    auto &cpu = g_cpu();
    ExecState *exec = exec_state_ptr();
    int total = 0;

    // Soft block chaining: execute compiled blocks until the cycle budget is
    // exhausted. Scheduler / RSP are batched by the outer system loop.
    while (total < budget) {
        if (cpu.delay_slot) {
            total += run_interpreter_fallback();
            break;
        }

        const uint32_t pc32 = static_cast<uint32_t>(cpu.get_pc64());
        // Hot path: KSEG0/1 direct map (almost all game code).
        uint32_t paddr;
        if (auto direct = Mmu::try_direct_map(pc32)) {
            paddr = *direct;
        } else {
            auto resolved = Mmu::resolve_vaddr_slow(pc32);
            if (!resolved.has_value()) {
                total += run_interpreter_fallback();
                break;
            }
            paddr = *resolved;
        }

        if (should_interpret_paddr(paddr)) {
            total += run_interpreter_fallback();
            break;
        }

        // Match Cpu::step: retire delay-slot flags before interrupt check.
        cpu.prev_delay_slot = cpu.delay_slot;
        cpu.delay_slot = false;

        if (cpu.should_service_interrupt()) {
            cpu.handle_exception(ExceptionCode::INTERRUPT, 0, false);
            total += static_cast<int>(CPU_CYCLES_PER_INST);
            break;
        }

        CompiledBlock *block = cache_.lookup(paddr);
        if (!block) {
            block = compile(pc32, paddr);
            if (!block) {
                total += run_interpreter_fallback();
                break;
            }
        }

        const int remaining = budget - total;
        if (total > 0 && block->num_insts > remaining)
            break;

        exec->aborted = false;
        exec->annul_delay_slot = false;
        const int taken = block->fn();
        const int got = taken > 0 ? taken : 1;
        total += got;
        if (exec->aborted)
            break;
    }

    return total > 0 ? total : 1;
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

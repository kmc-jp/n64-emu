#include "cpu/jit/jit.h"
#include "cpu/cpu.h"
#include "cpu/jit/helpers.h"
#include "cpu/jit/invalidate_hook.h"
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

CompiledBlock *Dynarec::compile(uint32_t vaddr, uint32_t paddr) {
    IrBlock ir;
    if (!translate_block(vaddr, paddr, ir))
        return nullptr;
    BlockFn fn = emit_block(ir, cache_);
    cache_.insert(paddr, fn, static_cast<uint16_t>(ir.ops.size()));
    return cache_.lookup(paddr);
}

int Dynarec::run(int budget) {
    if (budget <= 0)
        budget = 1;

    int total = 0;
    while (total < budget) {
        auto &cpu = g_cpu();

        // Match Cpu::step compare / interrupt servicing before fetch.
        if (cpu.cop0.reg.count == (cpu.cop0.reg.compare << 1)) {
            cpu.cop0.reg.cause.ip7 = true;
            N64System::check_interrupt();
        }
        if (cpu.should_service_interrupt()) {
            cpu.handle_exception(ExceptionCode::INTERRUPT, 0, false);
        }

        const uint32_t pc32 = static_cast<uint32_t>(cpu.get_pc64());
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(pc32);
        if (!paddr.has_value()) {
            // Same as interpreter: take TLB exception via step().
            total += run_interpreter_fallback();
            break;
        }

        CompiledBlock *block = cache_.lookup(paddr.value());
        if (!block) {
            block = compile(pc32, paddr.value());
            if (!block) {
                total += run_interpreter_fallback();
                continue;
            }
        }

        exec_state() = {};
        const int taken = block->fn();
        total += taken > 0 ? taken : 0;
        if (taken <= 0)
            break;
        if (exec_state().aborted)
            break;
    }

    return total > 0 ? total : 1;
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

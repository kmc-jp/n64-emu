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
    // Execute a single interpreter step or one compiled block per call so the
    // outer N64System loop can tick the scheduler (PI/SI DMA, timers, etc.).
    // Batching many DMEM IPL3 steps without scheduler breaks cart DMA.
    (void)budget;

    auto &cpu = g_cpu();

    const uint32_t pc32 = static_cast<uint32_t>(cpu.get_pc64());
    std::optional<uint32_t> paddr = Mmu::resolve_vaddr(pc32);
    if (!paddr.has_value())
        return run_interpreter_fallback();

    if (should_interpret_paddr(paddr.value()))
        return run_interpreter_fallback();

    // Match Cpu::step compare / interrupt servicing before a JIT block.
    // Interpreter fallback already does this inside Cpu::step().
    if (cpu.cop0.reg.count == (cpu.cop0.reg.compare << 1)) {
        cpu.cop0.reg.cause.ip7 = true;
        N64System::check_interrupt();
    }
    if (cpu.should_service_interrupt()) {
        cpu.handle_exception(ExceptionCode::INTERRUPT, 0, false);
        return static_cast<int>(CPU_CYCLES_PER_INST);
    }

    CompiledBlock *block = cache_.lookup(paddr.value());
    if (!block) {
        block = compile(pc32, paddr.value());
        if (!block)
            return run_interpreter_fallback();
    }

    exec_state() = {};
    const int taken = block->fn();
    if (exec_state().aborted)
        return taken > 0 ? taken : 1;
    return taken > 0 ? taken : 1;
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

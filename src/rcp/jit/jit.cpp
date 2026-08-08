#include "rcp/jit/jit.h"
#include "rcp/rsp.h"

namespace N64 {
namespace Rsp {
namespace Jit {

Dynarec Dynarec::instance_{};

Dynarec &Dynarec::get_instance() { return instance_; }

void Dynarec::reset() { cache_.clear(); }

void Dynarec::invalidate_all() { cache_.invalidate_all(); }

void Dynarec::invalidate_range(uint16_t addr, uint16_t length) {
    cache_.invalidate_range(addr, length);
}

int Dynarec::run_interpreter_fallback() {
    g_rsp().step();
    return g_rsp().halted() ? 0 : 1;
}

CompiledBlock *Dynarec::compile(uint16_t pc) {
    IrBlock ir;
    if (!translate_block(pc, ir))
        return nullptr;
    BlockFn fn = emit_block(ir, cache_);
    cache_.insert(pc, fn, static_cast<uint16_t>(ir.ops.size()));
    return cache_.lookup(pc);
}

int Dynarec::run(int budget) {
    if (budget < 1)
        budget = 1;

    Rsp &rsp = g_rsp();
    int total = 0;

    while (total < budget) {
        if (rsp.halted())
            break;

        const uint16_t pc = rsp.get_pc();
        CompiledBlock *block = cache_.lookup(pc);
        if (!block) {
            block = compile(pc);
            if (!block) {
                total += run_interpreter_fallback();
                continue;
            }
        }

        const int taken = block->fn();
        if (taken <= 0) {
            if (!rsp.halted())
                total += run_interpreter_fallback();
            else
                break;
            continue;
        }
        total += taken;
    }

    return total;
}

} // namespace Jit
} // namespace Rsp
} // namespace N64

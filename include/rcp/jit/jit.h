#ifndef RCP_JIT_JIT_H
#define RCP_JIT_JIT_H

#include "rcp/jit/code_cache.h"
#include "rcp/jit/ir.h"
#include <cstdint>

namespace N64 {
namespace Rsp {
namespace Jit {

class Dynarec {
  public:
    void reset();

    // Execute up to `budget` RSP instructions. Returns how many ran (>=0).
    int run(int budget);

    void invalidate_all();
    void invalidate_range(uint16_t addr, uint16_t length);

    uint32_t code_generation() const { return cache_.generation(); }

    static Dynarec &get_instance();

  private:
    Dynarec() = default;

    CompiledBlock *compile(uint16_t pc);
    int run_interpreter_fallback();

    CodeCache cache_;
    static Dynarec instance_;
};

inline Dynarec &g_dynarec() { return Dynarec::get_instance(); }

bool translate_block(uint16_t start_pc, IrBlock &out);
BlockFn emit_block(const IrBlock &block, CodeCache &cache);

} // namespace Jit
} // namespace Rsp
} // namespace N64

#endif

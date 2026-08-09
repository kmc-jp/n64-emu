#ifndef CPU_JIT_JIT_H
#define CPU_JIT_JIT_H

#include "cpu/jit/code_cache.h"
#include "cpu/jit/ir.h"
#include <cstdint>

namespace N64 {
namespace Cpu {
namespace Jit {

class Dynarec {
  public:
    void reset();

    // Soft-chain blocks up to `budget` cycles; advances RSP + scheduler after
    // each unit. Always returns >= 1 with matching machine advance.
    int run(int budget, bool rsp_thread);

    void invalidate_page(uint32_t paddr);
    void invalidate_range(uint32_t paddr, uint32_t length);

    bool page_has_code(uint32_t paddr) const {
        return cache_.page_has_code(paddr);
    }

    static Dynarec &get_instance();

  private:
    Dynarec() = default;

    CompiledBlock *compile(uint32_t vaddr, uint32_t paddr);
    int run_interpreter_fallback();

    CodeCache cache_;
    static Dynarec instance_;
};

inline Dynarec &g_dynarec() { return Dynarec::get_instance(); }

// Called from memory write / DMA paths.
void invalidate_code_page(uint32_t paddr);
void invalidate_code_range(uint32_t paddr, uint32_t length);

bool translate_block(uint32_t vaddr, uint32_t paddr, IrBlock &out);
BlockFn emit_block(const IrBlock &block, CodeCache &cache);

// Dumps + resets JIT timing counters (N64_PROFILE_FRAME or N64_PROFILE_JIT).
void jit_profile_dump();

} // namespace Jit
} // namespace Cpu
} // namespace N64

#endif

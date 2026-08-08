#include "cpu/jit/invalidate_hook.h"

namespace N64 {

namespace {
CodeInvalidateFn g_hook = nullptr;
}

void set_code_invalidate_hook(CodeInvalidateFn fn) { g_hook = fn; }

void maybe_invalidate_code(uint32_t paddr, uint32_t length) {
    if (g_hook)
        g_hook(paddr, length);
}

} // namespace N64

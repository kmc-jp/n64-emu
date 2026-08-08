#ifndef CPU_JIT_INVALIDATE_HOOK_H
#define CPU_JIT_INVALIDATE_HOOK_H

#include <cstdint>

namespace N64 {

using CodeInvalidateFn = void (*)(uint32_t paddr, uint32_t length);

void set_code_invalidate_hook(CodeInvalidateFn fn);
void maybe_invalidate_code(uint32_t paddr, uint32_t length);

} // namespace N64

#endif

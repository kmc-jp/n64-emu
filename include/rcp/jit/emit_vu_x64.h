#ifndef RCP_JIT_EMIT_VU_X64_H
#define RCP_JIT_EMIT_VU_X64_H

#include "rcp/jit/ir.h"
#include <cstddef>
#include <cstdint>
#include <xbyak/xbyak.h>

namespace N64 {
namespace Rsp {
namespace Jit {

bool vu_funct_inlinable(uint32_t funct);

// Emit SSE2 for a contiguous streak of inlinable VuCompute ops.
// Keeps ACC in xmm0/xmm1/xmm2 for the whole streak (load once / store once).
void emit_vu_inline_streak(Xbyak::CodeGenerator &g, uintptr_t vpr_base,
                           uintptr_t acc_l, uintptr_t acc_m, uintptr_t acc_h,
                           const IrOp *ops, size_t count);

} // namespace Jit
} // namespace Rsp
} // namespace N64

#endif

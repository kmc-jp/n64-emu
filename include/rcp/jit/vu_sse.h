#ifndef RCP_JIT_VU_SSE_H
#define RCP_JIT_VU_SSE_H

#include <cstdint>

namespace N64 {
namespace Rsp {
class Rsp;

namespace Jit {

// SSE2 VU helpers for RSP JIT. Returns true if funct was handled.
// vd/vs/vt/e/funct are already decoded (no opcode re-parse).
bool vu_sse_compute(Rsp &rsp, unsigned vd, unsigned vs, unsigned vt, unsigned e,
                    unsigned funct);

} // namespace Jit
} // namespace Rsp
} // namespace N64

#endif

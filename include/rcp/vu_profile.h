#ifndef RCP_VU_PROFILE_H
#define RCP_VU_PROFILE_H

#include <cstdint>

namespace N64 {
namespace Rsp {

// Enabled when N64_PROFILE_VU is set (non-empty, not "0").
bool vu_profile_enabled();
void vu_profile_compute(uint32_t inst, bool used_simd);
void vu_profile_lwc2(uint32_t inst);
void vu_profile_swc2(uint32_t inst);
void vu_profile_cop2_move(uint8_t sub);
void vu_profile_dump();

} // namespace Rsp
} // namespace N64

#endif

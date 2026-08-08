#ifndef RSP_SIMD_H
#define RSP_SIMD_H

#include "rcp/rsp.h"

#if N64_RSP_SIMD

#include <eve/module/core.hpp>
#include <eve/wide.hpp>
#include <array>
#include <cstdint>

namespace N64 {
namespace Rsp {
namespace Simd {

using Vu16 = eve::wide<std::uint16_t, eve::fixed<8>>;
using Vi16 = eve::wide<std::int16_t, eve::fixed<8>>;
using Vu32 = eve::wide<std::uint32_t, eve::fixed<8>>;
using Vi32 = eve::wide<std::int32_t, eve::fixed<8>>;
using Vi64 = eve::wide<std::int64_t, eve::fixed<8>>;

inline int broadcast_lane(int element, int dest_lane) {
    if (element < 2)
        return dest_lane;
    if (element < 4)
        return (dest_lane & ~1) | (element & 1);
    if (element < 8)
        return (dest_lane & ~3) | (element & 3);
    return element & 7;
}

inline Vu16 load_vu(const VuReg &r) { return Vu16(r.data()); }

inline void store_vu(VuReg &r, Vu16 v) { eve::store(v, r.data()); }

inline Vi16 as_i16(Vu16 v) { return eve::bit_cast(v, eve::as<Vi16>{}); }

inline Vu16 as_u16(Vi16 v) { return eve::bit_cast(v, eve::as<Vu16>{}); }

inline Vu16 broadcast_vt(const VuReg &vt, int element) {
    alignas(16) std::array<uint16_t, 8> tmp{};
    for (int i = 0; i < 8; i++)
        tmp[static_cast<size_t>(i)] =
            vt.lane(broadcast_lane(element, i));
    return Vu16(tmp.data());
}

inline Vu16 load_acc_h(Rsp &rsp) { return load_vu(rsp.acc_h()); }
inline Vu16 load_acc_m(Rsp &rsp) { return load_vu(rsp.acc_m()); }
inline Vu16 load_acc_l(Rsp &rsp) { return load_vu(rsp.acc_l()); }

inline void store_acc_h(Rsp &rsp, Vu16 v) { store_vu(rsp.acc_h(), v); }
inline void store_acc_m(Rsp &rsp, Vu16 v) { store_vu(rsp.acc_m(), v); }
inline void store_acc_l(Rsp &rsp, Vu16 v) { store_vu(rsp.acc_l(), v); }

inline void set_acc_low(Rsp &rsp, Vu16 lo) { store_acc_l(rsp, lo); }

inline void mullo_mulhi_i16(Vi16 a, Vi16 b, Vu16 &lo, Vi16 &hi) {
    Vi32 p = eve::convert(a, eve::as<std::int32_t>{}) *
             eve::convert(b, eve::as<std::int32_t>{});
    lo = eve::convert(eve::bit_and(p, Vi32(0xFFFF)),
                      eve::as<std::uint16_t>{});
    hi = eve::convert(p >> 16, eve::as<std::int16_t>{});
}

inline Vu16 mulhi_epu16(Vu16 a, Vu16 b) {
    Vu32 p = eve::convert(a, eve::as<std::uint32_t>{}) *
             eve::convert(b, eve::as<std::uint32_t>{});
    return eve::convert(p >> 16, eve::as<std::uint16_t>{});
}

// Unsigned wrap add; carry is 0xFFFF where overflow occurred (CEN64 style).
inline Vu16 add_u16_carry_mask(Vu16 a, Vu16 b, Vu16 &sum) {
    sum = a + b;
    auto carry = sum < a;
    return eve::if_else(carry, Vu16(0xFFFF), Vu16(0));
}

inline Vu16 sclamp_md_hi(Vu16 md, Vu16 hi) {
    Vi32 combined = (eve::convert(as_i16(hi), eve::as<std::int32_t>{}) << 16) |
                    eve::convert(md, eve::as<std::int32_t>{});
    return as_u16(eve::convert(eve::clamp(combined, Vi32(-32768), Vi32(32767)),
                               eve::as<std::int16_t>{}));
}

inline Vu16 uclamp_lo_md_hi(Vu16 lo, Vu16 md, Vu16 hi) {
    Vi16 hi_i = as_i16(hi);
    Vi16 md_i = as_i16(md);
    Vi16 hi_neg = hi_i >> 15; // 0 or -1
    auto ok = (hi_i == hi_neg) && ((md_i >> 15) == hi_neg);
    Vu16 clamped = eve::if_else(hi_neg == Vi16(0), Vu16(0xFFFF), Vu16(0));
    return eve::if_else(ok, lo, clamped);
}

inline uint16_t logical_to_bits(eve::logical<Vi16> mask) {
    return static_cast<uint16_t>(mask.bitmap().to_ulong() & 0xFFu);
}

inline uint16_t logical_to_bits(eve::logical<Vu16> mask) {
    return static_cast<uint16_t>(mask.bitmap().to_ulong() & 0xFFu);
}

inline Vi16 vco_lo_as_i16(uint16_t vco) {
    alignas(16) std::array<std::int16_t, 8> tmp{};
    for (int i = 0; i < 8; i++)
        tmp[static_cast<size_t>(i)] = (vco >> i) & 1;
    return Vi16(tmp.data());
}

inline Vu16 vco_lo_as_u16(uint16_t vco) {
    return as_u16(vco_lo_as_i16(vco));
}

inline Vu16 vcc_lo_as_u16(uint16_t vcc) {
    alignas(16) std::array<std::uint16_t, 8> tmp{};
    for (int i = 0; i < 8; i++)
        tmp[static_cast<size_t>(i)] =
            static_cast<std::uint16_t>((vcc >> i) & 1);
    return Vu16(tmp.data());
}

inline Vu16 vco_hi_as_u16(uint16_t vco) {
    alignas(16) std::array<std::uint16_t, 8> tmp{};
    for (int i = 0; i < 8; i++)
        tmp[static_cast<size_t>(i)] =
            static_cast<std::uint16_t>((vco >> (i + 8)) & 1);
    return Vu16(tmp.data());
}

// Kept for any remaining Vi64 helpers; prefer H/M/L paths.
inline Vu16 clamp_signed_to_u16(Vi64 v) {
    Vi64 c = eve::clamp(v, Vi64(-32768), Vi64(32767));
    return as_u16(eve::convert(c, eve::as<std::int16_t>{}));
}

} // namespace Simd
} // namespace Rsp
} // namespace N64

#endif // N64_RSP_SIMD

#endif

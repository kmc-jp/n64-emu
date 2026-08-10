#ifndef RSP_SIMD_H
#define RSP_SIMD_H

#include "rcp/rsp.h"

#if N64_RSP_SIMD

#include <array>
#include <cstdint>
#include <cstring>
#include <eve/module/core.hpp>
#include <eve/wide.hpp>
#include <tmmintrin.h> // SSSE3: _mm_shuffle_epi8
#include <smmintrin.h> // SSE4.1: _mm_blendv_epi8

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

inline __m128i to_m128(Vu16 v) {
    __m128i m;
    std::memcpy(&m, &v, sizeof(m));
    return m;
}

inline Vu16 from_m128(__m128i m) {
    Vu16 v;
    std::memcpy(&v, &m, sizeof(v));
    return v;
}

inline Vu16 load_vu(const VuReg &r) {
    return from_m128(
        _mm_loadu_si128(reinterpret_cast<const __m128i *>(r.data())));
}

inline void store_vu(VuReg &r, Vu16 v) {
    _mm_storeu_si128(reinterpret_cast<__m128i *>(r.data()), to_m128(v));
}

inline Vi16 as_i16(Vu16 v) { return eve::bit_cast(v, eve::as<Vi16>{}); }

inline Vu16 as_u16(Vi16 v) { return eve::bit_cast(v, eve::as<Vu16>{}); }

// Build SSSE3 pshufb controls from broadcast_lane() so VE matches the scalar
// path. VuReg stores N64 elem i at bytes 2*i..2*i+1 (low address = elem0).
inline const __m128i *ve_shuffle_table() {
    static __m128i table[16];
    static bool ready = false;
    if (!ready) {
        for (int ve = 0; ve < 16; ve++) {
            alignas(16) std::uint8_t ctrl[16];
            for (int dest = 0; dest < 8; dest++) {
                const int src = broadcast_lane(ve, dest);
                ctrl[2 * dest] = static_cast<std::uint8_t>(2 * src);
                ctrl[2 * dest + 1] = static_cast<std::uint8_t>(2 * src + 1);
            }
            table[ve] = _mm_load_si128(reinterpret_cast<const __m128i *>(ctrl));
        }
        ready = true;
    }
    return table;
}

inline Vu16 broadcast_vt(const VuReg &vt, int element) {
    const __m128i v =
        _mm_loadu_si128(reinterpret_cast<const __m128i *>(vt.data()));
    return from_m128(_mm_shuffle_epi8(v, ve_shuffle_table()[element & 15]));
}

inline Vu16 load_acc_h(Rsp &rsp) { return load_vu(rsp.acc_h()); }
inline Vu16 load_acc_m(Rsp &rsp) { return load_vu(rsp.acc_m()); }
inline Vu16 load_acc_l(Rsp &rsp) { return load_vu(rsp.acc_l()); }

inline void store_acc_h(Rsp &rsp, Vu16 v) { store_vu(rsp.acc_h(), v); }
inline void store_acc_m(Rsp &rsp, Vu16 v) { store_vu(rsp.acc_m(), v); }
inline void store_acc_l(Rsp &rsp, Vu16 v) { store_vu(rsp.acc_l(), v); }

inline void set_acc_low(Rsp &rsp, Vu16 lo) { store_acc_l(rsp, lo); }

inline void mullo_mulhi_i16(Vi16 a, Vi16 b, Vu16 &lo, Vi16 &hi) {
    const __m128i ma = to_m128(as_u16(a));
    const __m128i mb = to_m128(as_u16(b));
    lo = from_m128(_mm_mullo_epi16(ma, mb));
    hi = as_i16(from_m128(_mm_mulhi_epi16(ma, mb)));
}

inline Vu16 mulhi_epu16(Vu16 a, Vu16 b) {
    return from_m128(_mm_mulhi_epu16(to_m128(a), to_m128(b)));
}

inline Vu16 mullo_epi16(Vu16 a, Vu16 b) {
    return from_m128(_mm_mullo_epi16(to_m128(a), to_m128(b)));
}

// Unsigned wrap add; carry lanes become 0xFFFF on overflow.
inline Vu16 add_u16_carry_mask(Vu16 a, Vu16 b, Vu16 &sum) {
    const __m128i ma = to_m128(a);
    const __m128i mb = to_m128(b);
    const __m128i s = _mm_add_epi16(ma, mb);
    const __m128i overflow = _mm_cmpeq_epi16(_mm_adds_epu16(ma, mb), s);
    const __m128i carry = _mm_cmpeq_epi16(overflow, _mm_setzero_si128());
    sum = from_m128(s);
    return from_m128(carry);
}

inline Vu16 sclamp_md_hi(Vu16 md, Vu16 hi) {
    const __m128i m = to_m128(md);
    const __m128i h = to_m128(hi);
    const __m128i lo_packed = _mm_unpacklo_epi16(m, h);
    const __m128i hi_packed = _mm_unpackhi_epi16(m, h);
    return from_m128(_mm_packs_epi32(lo_packed, hi_packed));
}

inline Vu16 uclamp_lo_md_hi(Vu16 lo, Vu16 md, Vu16 hi) {
    const __m128i accl = to_m128(lo);
    const __m128i accm = to_m128(md);
    const __m128i acch = to_m128(hi);
    const __m128i nhi = _mm_srai_epi16(acch, 15);
    const __m128i nmd = _mm_srai_epi16(accm, 15);
    const __m128i shi = _mm_cmpeq_epi16(nhi, acch);
    const __m128i smd = _mm_cmpeq_epi16(nhi, nmd);
    const __m128i cmask = _mm_and_si128(smd, shi);
    const __m128i cval = _mm_cmpeq_epi16(nhi, _mm_setzero_si128());
    return from_m128(_mm_blendv_epi8(cval, accl, cmask));
}

inline uint16_t logical_to_bits(eve::logical<Vi16> mask) {
    return static_cast<uint16_t>(mask.bitmap().to_ulong() & 0xFFu);
}

inline uint16_t logical_to_bits(eve::logical<Vu16> mask) {
    return static_cast<uint16_t>(mask.bitmap().to_ulong() & 0xFFu);
}

inline Vi16 vco_lo_as_i16(uint16_t vco) {
    // 0 or 1 per lane — VADD/VSUB add this as carry-in.
    const __m128i bits = _mm_set1_epi16(static_cast<int>(vco));
    const __m128i lane = _mm_set_epi16(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02,
                                       0x01);
    const __m128i hit = _mm_cmpeq_epi16(_mm_and_si128(bits, lane), lane);
    return as_i16(from_m128(_mm_and_si128(hit, _mm_set1_epi16(1))));
}

inline Vu16 vco_lo_as_u16(uint16_t vco) {
    // 0 or 1 — compared with != 0 in VLT/VEQ paths.
    return as_u16(vco_lo_as_i16(vco));
}

inline Vu16 vcc_lo_as_u16(uint16_t vcc) {
    const __m128i bits = _mm_set1_epi16(static_cast<int>(vcc));
    const __m128i lane = _mm_set_epi16(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02,
                                       0x01);
    // Full 0xFFFF masks; VMRG tests != 0.
    return from_m128(_mm_cmpeq_epi16(_mm_and_si128(bits, lane), lane));
}

inline Vu16 vcc_hi_as_u16(uint16_t vcc) {
    return vcc_lo_as_u16(static_cast<uint16_t>(vcc >> 8));
}

inline Vu16 vco_hi_as_u16(uint16_t vco) {
    // 0 or 1 to match vco_lo_as_u16.
    return as_u16(vco_lo_as_i16(static_cast<uint16_t>(vco >> 8)));
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

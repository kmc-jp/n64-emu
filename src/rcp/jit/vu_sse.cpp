#include "rcp/jit/vu_sse.h"
#include "rcp/rsp.h"
#include <emmintrin.h>

namespace N64 {
namespace Rsp {
namespace Jit {
namespace {

__m128i load_vu(const VuReg &r) {
    return _mm_load_si128(reinterpret_cast<const __m128i *>(r.data()));
}

void store_vu(VuReg &r, __m128i v) {
    _mm_store_si128(reinterpret_cast<__m128i *>(r.data()), v);
}

__m128i shuffle_vt(const VuReg &vt, unsigned element) {
    __m128i v = load_vu(vt);
    switch (element) {
    case 0:
    case 1:
        return v;
    case 2:
        v = _mm_shufflelo_epi16(v, _MM_SHUFFLE(2, 2, 0, 0));
        return _mm_shufflehi_epi16(v, _MM_SHUFFLE(2, 2, 0, 0));
    case 3:
        v = _mm_shufflelo_epi16(v, _MM_SHUFFLE(3, 3, 1, 1));
        return _mm_shufflehi_epi16(v, _MM_SHUFFLE(3, 3, 1, 1));
    case 4:
    case 5:
    case 6:
    case 7: {
        __m128i t = _mm_setzero_si128();
        t = _mm_insert_epi16(t, vt.lane(static_cast<int>(element - 4)), 0);
        t = _mm_insert_epi16(t, vt.lane(static_cast<int>(element)), 1);
        t = _mm_shufflelo_epi16(t, _MM_SHUFFLE(1, 1, 0, 0));
        return _mm_shuffle_epi32(t, _MM_SHUFFLE(1, 1, 0, 0));
    }
    default: {
        __m128i t = _mm_setzero_si128();
        t = _mm_insert_epi16(t, vt.lane(static_cast<int>(element - 8)), 0);
        t = _mm_unpacklo_epi16(t, t);
        return _mm_shuffle_epi32(t, _MM_SHUFFLE(0, 0, 0, 0));
    }
    }
}

__m128i sclamp_md_hi(__m128i md, __m128i hi) {
    __m128i l = _mm_unpacklo_epi16(md, hi);
    __m128i h = _mm_unpackhi_epi16(md, hi);
    return _mm_packs_epi32(l, h);
}

__m128i uclamp_lo_md_hi(__m128i lo, __m128i md, __m128i hi, __m128i zero) {
    __m128i hi_neg = _mm_srai_epi16(hi, 15);
    __m128i md_neg = _mm_srai_epi16(md, 15);
    __m128i hi_ok = _mm_cmpeq_epi16(hi_neg, hi);
    __m128i md_ok = _mm_cmpeq_epi16(hi_neg, md_neg);
    __m128i ok = _mm_and_si128(hi_ok, md_ok);
    __m128i clamped = _mm_cmpeq_epi16(hi_neg, zero);
    __m128i keep = _mm_and_si128(ok, lo);
    __m128i repl = _mm_andnot_si128(ok, clamped);
    return _mm_or_si128(keep, repl);
}

// Carry-out as 0xFFFF lanes where unsigned add overflowed.
void acc_add_u16(__m128i &dst, __m128i src, __m128i zero, __m128i &carry_ffff) {
    __m128i sat = _mm_adds_epu16(dst, src);
    dst = _mm_add_epi16(dst, src);
    carry_ffff = _mm_cmpeq_epi16(dst, sat);
    carry_ffff = _mm_cmpeq_epi16(carry_ffff, zero);
}

__m128i op_vmadh(__m128i vs, __m128i vt, __m128i zero, __m128i &acc_l,
                 __m128i &acc_m, __m128i &acc_h, bool accumulate) {
    (void)acc_l;
    __m128i lo = _mm_mullo_epi16(vs, vt);
    __m128i hi = _mm_mulhi_epi16(vs, vt);
    if (!accumulate) {
        acc_l = zero;
        acc_m = lo;
        acc_h = hi;
        return sclamp_md_hi(acc_m, acc_h);
    }
    __m128i ov;
    acc_add_u16(acc_m, lo, zero, ov);
    hi = _mm_sub_epi16(hi, ov);
    acc_h = _mm_add_epi16(acc_h, hi);
    return sclamp_md_hi(acc_m, acc_h);
}

__m128i op_vmadn(__m128i vs, __m128i vt, __m128i zero, __m128i &acc_l,
                 __m128i &acc_m, __m128i &acc_h, bool accumulate) {
    __m128i lo = _mm_mullo_epi16(vs, vt);
    __m128i hi = _mm_mulhi_epu16(vs, vt);
    __m128i sign = _mm_srai_epi16(vt, 15);
    hi = _mm_sub_epi16(hi, _mm_and_si128(vs, sign));
    if (!accumulate) {
        acc_l = lo;
        acc_m = hi;
        acc_h = _mm_srai_epi16(hi, 15);
        return lo;
    }
    __m128i ov;
    acc_add_u16(acc_l, lo, zero, ov);
    hi = _mm_sub_epi16(hi, ov);
    acc_add_u16(acc_m, hi, zero, ov);
    acc_h = _mm_add_epi16(acc_h, _mm_srai_epi16(hi, 15));
    acc_h = _mm_sub_epi16(acc_h, ov);
    return uclamp_lo_md_hi(acc_l, acc_m, acc_h, zero);
}

__m128i op_vmadm(__m128i vs, __m128i vt, __m128i zero, __m128i &acc_l,
                 __m128i &acc_m, __m128i &acc_h, bool accumulate) {
    __m128i lo = _mm_mullo_epi16(vs, vt);
    __m128i hi = _mm_mulhi_epu16(vs, vt);
    __m128i sign = _mm_srai_epi16(vs, 15);
    hi = _mm_sub_epi16(hi, _mm_and_si128(vt, sign));
    if (!accumulate) {
        acc_l = lo;
        acc_m = hi;
        acc_h = _mm_srai_epi16(hi, 15);
        return hi;
    }
    __m128i ov;
    acc_add_u16(acc_l, lo, zero, ov);
    hi = _mm_sub_epi16(hi, ov);
    acc_add_u16(acc_m, hi, zero, ov);
    acc_h = _mm_add_epi16(acc_h, _mm_srai_epi16(hi, 15));
    acc_h = _mm_sub_epi16(acc_h, ov);
    return sclamp_md_hi(acc_m, acc_h);
}

__m128i op_vmadl(__m128i vs, __m128i vt, __m128i zero, __m128i &acc_l,
                 __m128i &acc_m, __m128i &acc_h, bool accumulate) {
    __m128i hi = _mm_mulhi_epu16(vs, vt);
    if (!accumulate) {
        acc_l = hi;
        acc_m = zero;
        acc_h = zero;
        return hi;
    }
    __m128i ov;
    acc_add_u16(acc_l, hi, zero, ov);
    __m128i carry = _mm_sub_epi16(zero, ov);
    acc_add_u16(acc_m, carry, zero, ov);
    acc_h = _mm_sub_epi16(acc_h, ov);
    return uclamp_lo_md_hi(acc_l, acc_m, acc_h, zero);
}

__m128i op_vmulf(__m128i vs, __m128i vt, __m128i zero, __m128i &acc_l,
                 __m128i &acc_m, __m128i &acc_h) {
    __m128i lo = _mm_mullo_epi16(vs, vt);
    __m128i round = _mm_cmpeq_epi16(zero, zero);
    __m128i sign1 = _mm_srli_epi16(lo, 15);
    lo = _mm_add_epi16(lo, lo);
    round = _mm_slli_epi16(round, 15);
    __m128i hi = _mm_mulhi_epi16(vs, vt);
    __m128i sign2 = _mm_srli_epi16(lo, 15);
    acc_l = _mm_add_epi16(round, lo);
    sign1 = _mm_add_epi16(sign1, sign2);
    hi = _mm_slli_epi16(hi, 1);
    __m128i eq = _mm_cmpeq_epi16(vs, vt);
    acc_m = _mm_add_epi16(hi, sign1);
    __m128i neg = _mm_srai_epi16(acc_m, 15);
    __m128i eq_and_neg = _mm_and_si128(eq, neg);
    acc_h = _mm_andnot_si128(eq, neg);
    return _mm_add_epi16(acc_m, eq_and_neg);
}

__m128i op_vmacf(__m128i vs, __m128i vt, __m128i zero, __m128i &acc_l,
                 __m128i &acc_m, __m128i &acc_h) {
    __m128i lo = _mm_mullo_epi16(vs, vt);
    __m128i hi = _mm_mulhi_epi16(vs, vt);
    __m128i md = _mm_slli_epi16(hi, 1);
    __m128i carry = _mm_srli_epi16(lo, 15);
    hi = _mm_srai_epi16(hi, 15);
    md = _mm_or_si128(md, carry);
    lo = _mm_slli_epi16(lo, 1);

    __m128i ov;
    acc_add_u16(acc_l, lo, zero, ov);
    md = _mm_sub_epi16(md, ov);
    carry = _mm_and_si128(_mm_cmpeq_epi16(md, zero), ov);
    hi = _mm_sub_epi16(hi, carry);

    acc_add_u16(acc_m, md, zero, ov);
    acc_h = _mm_add_epi16(acc_h, hi);
    acc_h = _mm_sub_epi16(acc_h, ov);
    return sclamp_md_hi(acc_m, acc_h);
}

} // namespace

bool vu_sse_compute(Rsp &rsp, unsigned vd, unsigned vs, unsigned vt, unsigned e,
                    unsigned funct) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i vs_v = load_vu(rsp.vreg(static_cast<int>(vs)));
    const __m128i vt_v = shuffle_vt(rsp.vreg(static_cast<int>(vt)), e);

    __m128i acc_l = load_vu(rsp.acc_l());
    __m128i acc_m = load_vu(rsp.acc_m());
    __m128i acc_h = load_vu(rsp.acc_h());
    __m128i out;

    switch (funct) {
    case 0x00: // VMULF
        out = op_vmulf(vs_v, vt_v, zero, acc_l, acc_m, acc_h);
        break;
    case 0x04: // VMUDL
        out = op_vmadl(vs_v, vt_v, zero, acc_l, acc_m, acc_h, false);
        break;
    case 0x05: // VMUDM
        out = op_vmadm(vs_v, vt_v, zero, acc_l, acc_m, acc_h, false);
        break;
    case 0x06: // VMUDN
        out = op_vmadn(vs_v, vt_v, zero, acc_l, acc_m, acc_h, false);
        break;
    case 0x07: // VMUDH
        out = op_vmadh(vs_v, vt_v, zero, acc_l, acc_m, acc_h, false);
        break;
    case 0x08: // VMACF
        out = op_vmacf(vs_v, vt_v, zero, acc_l, acc_m, acc_h);
        break;
    case 0x0C: // VMADL
        out = op_vmadl(vs_v, vt_v, zero, acc_l, acc_m, acc_h, true);
        break;
    case 0x0D: // VMADM
        out = op_vmadm(vs_v, vt_v, zero, acc_l, acc_m, acc_h, true);
        break;
    case 0x0E: // VMADN
        out = op_vmadn(vs_v, vt_v, zero, acc_l, acc_m, acc_h, true);
        break;
    case 0x0F: // VMADH
        out = op_vmadh(vs_v, vt_v, zero, acc_l, acc_m, acc_h, true);
        break;
    default:
        return false;
    }

    store_vu(rsp.acc_l(), acc_l);
    store_vu(rsp.acc_m(), acc_m);
    store_vu(rsp.acc_h(), acc_h);
    store_vu(rsp.vreg(static_cast<int>(vd)), out);
    return true;
}

} // namespace Jit
} // namespace Rsp
} // namespace N64

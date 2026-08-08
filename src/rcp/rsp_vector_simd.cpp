#include "rcp/rsp.h"
#include "rcp/rsp_rom.h"
#include "rcp/rsp_simd.h"
#include "rcp/vu_profile.h"
#include "utils/log.h"

#if defined(N64_RSP_JIT)
#include "rcp/jit/vu_sse.h"
#endif

#if N64_RSP_SIMD

namespace N64 {
namespace Rsp {

using namespace Simd;

namespace {

Vu16 mask_from_logical(eve::logical<Vi16> c) {
    return as_u16(eve::if_else(c, Vi16(-1), Vi16(0)));
}

Vu16 mask_from_logical(eve::logical<Vu16> c) {
    return eve::if_else(c, Vu16(0xFFFF), Vu16(0));
}

// CEN64/parallel-rsp: accumulate u16 with carry-out as 0xFFFF mask.
void acc_add_u16(Vu16 &dst, Vu16 src, Vu16 &carry_ffff) {
    Vu16 sum;
    carry_ffff = add_u16_carry_mask(dst, src, sum);
    dst = sum;
}

Vu16 simd_vmulf_vmulu(Vi16 vs, Vi16 vt, bool vmulu, Vu16 &acc_l, Vu16 &acc_m,
                      Vu16 &acc_h) {
    Vu16 lo;
    Vi16 hi_i;
    mullo_mulhi_i16(vs, vt, lo, hi_i);

    Vu16 sign1 = lo >> 15;
    lo = lo + lo;
    Vu16 sign2 = lo >> 15;
    acc_l = Vu16(0x8000) + lo;
    sign1 = sign1 + sign2;

    Vu16 hi = as_u16(hi_i) + as_u16(hi_i);
    const Vu16 eq = mask_from_logical(vs == vt);
    acc_m = hi + sign1;
    const Vu16 neg = as_u16(as_i16(acc_m) >> 15);

    if (vmulu) {
        acc_h = eve::bit_and(eve::bit_not(eq), neg);
        const Vu16 t = eve::bit_or(acc_m, neg);
        return eve::bit_and(eve::bit_not(acc_h), t);
    }
    const Vu16 eq_and_neg = eve::bit_and(eq, neg);
    acc_h = eve::bit_and(eve::bit_not(eq), neg);
    return acc_m + eq_and_neg;
}

Vu16 simd_vmacf_vmacu(Vi16 vs, Vi16 vt, bool vmacu, Vu16 &acc_l, Vu16 &acc_m,
                      Vu16 &acc_h) {
    Vu16 lo;
    Vi16 hi_i;
    mullo_mulhi_i16(vs, vt, lo, hi_i);

    Vu16 md = as_u16(hi_i) + as_u16(hi_i);
    Vu16 carry = lo >> 15;
    Vi16 hi = hi_i >> 15;
    md = eve::bit_or(md, carry);
    lo = lo + lo;

    Vu16 ov;
    acc_add_u16(acc_l, lo, ov);
    md = md - ov;
    carry = mask_from_logical((md == Vu16(0)) && (ov == Vu16(0xFFFF)));
    hi = hi - as_i16(carry);

    acc_add_u16(acc_m, md, ov);
    acc_h = as_u16(as_i16(acc_h) + hi - as_i16(ov));

    if (vmacu) {
        const Vu16 overflow_hi = as_u16(as_i16(acc_h) >> 15);
        const Vu16 overflow_md = as_u16(as_i16(acc_m) >> 15);
        Vu16 md_out = eve::bit_or(overflow_md, acc_m);
        md_out = eve::bit_and(eve::bit_not(overflow_hi), md_out);
        const Vu16 pos_hi = mask_from_logical(as_i16(acc_h) > Vi16(0));
        return eve::bit_or(pos_hi, md_out);
    }
    return sclamp_md_hi(acc_m, acc_h);
}

Vu16 simd_vmudl_vmadl(Vu16 vs, Vu16 vt, bool vmadl, Vu16 &acc_l, Vu16 &acc_m,
                      Vu16 &acc_h) {
    Vu16 hi = mulhi_epu16(vs, vt);
    if (!vmadl) {
        acc_l = hi;
        acc_m = Vu16(0);
        acc_h = Vu16(0);
        return hi;
    }
    Vu16 ov;
    acc_add_u16(acc_l, hi, ov);
    Vu16 carry = Vu16(0) - ov; // 0 or 1
    acc_add_u16(acc_m, carry, ov);
    acc_h = acc_h - ov;
    return uclamp_lo_md_hi(acc_l, acc_m, acc_h);
}

Vu16 simd_vmudm_vmadm(Vi16 vs, Vu16 vt, bool vmadm, Vu16 &acc_l, Vu16 &acc_m,
                      Vu16 &acc_h) {
    // vs signed, vt unsigned — start from u16*u16 then fix high.
    Vu16 lo;
    Vi16 unused;
    mullo_mulhi_i16(vs, as_i16(vt), lo, unused); // mullo bits ignore signedness
    (void)unused;
    Vu16 hi = mulhi_epu16(as_u16(vs), vt);
    Vu16 sign = as_u16(vs >> 15);
    Vu16 vt_if_neg = eve::bit_and(vt, sign);
    hi = hi - vt_if_neg;

    if (!vmadm) {
        acc_l = lo;
        acc_m = hi;
        acc_h = as_u16(as_i16(hi) >> 15);
        return hi;
    }
    Vu16 ov;
    acc_add_u16(acc_l, lo, ov);
    hi = hi - ov;
    acc_add_u16(acc_m, hi, ov);
    acc_h = as_u16(as_i16(acc_h) + (as_i16(hi) >> 15) - as_i16(ov));
    return sclamp_md_hi(acc_m, acc_h);
}

Vu16 simd_vmudn_vmadn(Vu16 vs_u, Vi16 vt, bool vmadn, Vu16 &acc_l, Vu16 &acc_m,
                      Vu16 &acc_h) {
    Vu16 vs = vs_u;
    Vu16 lo;
    Vi16 unused;
    mullo_mulhi_i16(as_i16(vs), vt, lo, unused);
    (void)unused;
    Vu16 hi = mulhi_epu16(vs, as_u16(vt));
    Vu16 sign = as_u16(vt >> 15);
    Vu16 vs_if_neg = eve::bit_and(vs, sign);
    hi = hi - vs_if_neg;

    if (!vmadn) {
        acc_l = lo;
        acc_m = hi;
        acc_h = as_u16(as_i16(hi) >> 15);
        return lo;
    }
    Vu16 ov;
    acc_add_u16(acc_l, lo, ov);
    hi = hi - ov;
    acc_add_u16(acc_m, hi, ov);
    acc_h = as_u16(as_i16(acc_h) + (as_i16(hi) >> 15) - as_i16(ov));
    return uclamp_lo_md_hi(acc_l, acc_m, acc_h);
}

Vu16 simd_vmudh_vmadh(Vi16 vs, Vi16 vt, bool vmadh, Vu16 &acc_l, Vu16 &acc_m,
                      Vu16 &acc_h) {
    Vu16 lo;
    Vi16 hi_i;
    mullo_mulhi_i16(vs, vt, lo, hi_i);
    if (!vmadh) {
        acc_l = Vu16(0);
        acc_m = lo;
        acc_h = as_u16(hi_i);
        return sclamp_md_hi(acc_m, acc_h);
    }
    Vu16 ov;
    acc_add_u16(acc_m, lo, ov);
    Vi16 hi = hi_i - as_i16(ov);
    acc_h = as_u16(as_i16(acc_h) + hi);
    return sclamp_md_hi(acc_m, acc_h);
}

bool try_execute_simd(Rsp &rsp, uint32_t inst) {
    const int vd = (inst >> 6) & 0x1F;
    const int vs = (inst >> 11) & 0x1F;
    const int vt = (inst >> 16) & 0x1F;
    const int element = (inst >> 21) & 0xF;
    const int funct = inst & 0x3F;

    auto &dest = rsp.vreg(vd);
    const auto &src_s = rsp.vreg(vs);
    const auto &src_t = rsp.vreg(vt);

    const Vu16 vs_u = load_vu(src_s);
    const Vu16 vt_u = broadcast_vt(src_t, element);
    const Vi16 vs_i = as_i16(vs_u);
    const Vi16 vt_i = as_i16(vt_u);

    switch (funct) {
    case 0x00:   // VMULF
    case 0x01: { // VMULU
        Vu16 al, am, ah;
        Vu16 out = simd_vmulf_vmulu(vs_i, vt_i, funct == 0x01, al, am, ah);
        store_acc_l(rsp, al);
        store_acc_m(rsp, am);
        store_acc_h(rsp, ah);
        store_vu(dest, out);
        return true;
    }

    case 0x08:   // VMACF
    case 0x09: { // VMACU
        Vu16 al = load_acc_l(rsp);
        Vu16 am = load_acc_m(rsp);
        Vu16 ah = load_acc_h(rsp);
        Vu16 out = simd_vmacf_vmacu(vs_i, vt_i, funct == 0x09, al, am, ah);
        store_acc_l(rsp, al);
        store_acc_m(rsp, am);
        store_acc_h(rsp, ah);
        store_vu(dest, out);
        return true;
    }

    case 0x04:   // VMUDL
    case 0x0C: { // VMADL
        Vu16 al = load_acc_l(rsp);
        Vu16 am = load_acc_m(rsp);
        Vu16 ah = load_acc_h(rsp);
        Vu16 out = simd_vmudl_vmadl(vs_u, vt_u, funct == 0x0C, al, am, ah);
        store_acc_l(rsp, al);
        store_acc_m(rsp, am);
        store_acc_h(rsp, ah);
        store_vu(dest, out);
        return true;
    }

    case 0x05:   // VMUDM
    case 0x0D: { // VMADM
        Vu16 al = load_acc_l(rsp);
        Vu16 am = load_acc_m(rsp);
        Vu16 ah = load_acc_h(rsp);
        Vu16 out = simd_vmudm_vmadm(vs_i, vt_u, funct == 0x0D, al, am, ah);
        store_acc_l(rsp, al);
        store_acc_m(rsp, am);
        store_acc_h(rsp, ah);
        store_vu(dest, out);
        return true;
    }

    case 0x06:   // VMUDN
    case 0x0E: { // VMADN
        Vu16 al = load_acc_l(rsp);
        Vu16 am = load_acc_m(rsp);
        Vu16 ah = load_acc_h(rsp);
        Vu16 out = simd_vmudn_vmadn(vs_u, vt_i, funct == 0x0E, al, am, ah);
        store_acc_l(rsp, al);
        store_acc_m(rsp, am);
        store_acc_h(rsp, ah);
        store_vu(dest, out);
        return true;
    }

    case 0x07:   // VMUDH
    case 0x0F: { // VMADH
        Vu16 al = load_acc_l(rsp);
        Vu16 am = load_acc_m(rsp);
        Vu16 ah = load_acc_h(rsp);
        Vu16 out = simd_vmudh_vmadh(vs_i, vt_i, funct == 0x0F, al, am, ah);
        store_acc_l(rsp, al);
        store_acc_m(rsp, am);
        store_acc_h(rsp, ah);
        store_vu(dest, out);
        return true;
    }

    case 0x10: { // VADD
        Vi32 sum =
            eve::convert(vs_i, eve::as<std::int32_t>{}) +
            eve::convert(vt_i, eve::as<std::int32_t>{}) +
            eve::convert(vco_lo_as_i16(rsp.vco_ref()), eve::as<std::int32_t>{});
        Vu16 raw = eve::convert(sum, eve::as<std::uint16_t>{});
        set_acc_low(rsp, raw);
        store_vu(dest, clamp_signed_to_u16(
                           eve::convert(sum, eve::as<std::int64_t>{})));
        rsp.vco_ref() = 0;
        return true;
    }

    case 0x11: { // VSUB
        Vi32 diff =
            eve::convert(vs_i, eve::as<std::int32_t>{}) -
            eve::convert(vt_i, eve::as<std::int32_t>{}) -
            eve::convert(vco_lo_as_i16(rsp.vco_ref()), eve::as<std::int32_t>{});
        Vu16 raw = eve::convert(diff, eve::as<std::uint16_t>{});
        set_acc_low(rsp, raw);
        store_vu(dest, clamp_signed_to_u16(
                           eve::convert(diff, eve::as<std::int64_t>{})));
        rsp.vco_ref() = 0;
        return true;
    }

    case 0x13: { // VABS
        auto s_neg = vs_i < Vi16(0);
        auto s_zero = vs_i == Vi16(0);
        auto t_min = vt_u == Vu16(0x8000);
        Vu16 neg_t = as_u16(-vt_i);
        Vu16 dest_neg = eve::if_else(t_min, Vu16(0x7FFF), neg_t);
        Vu16 acc_neg = eve::if_else(t_min, Vu16(0x8000), neg_t);
        Vu16 out =
            eve::if_else(s_neg, dest_neg, eve::if_else(s_zero, Vu16(0), vt_u));
        Vu16 acc =
            eve::if_else(s_neg, acc_neg, eve::if_else(s_zero, Vu16(0), vt_u));
        store_vu(dest, out);
        set_acc_low(rsp, acc);
        return true;
    }

    case 0x14: { // VADDC
        Vu32 sum = eve::convert(vs_u, eve::as<std::uint32_t>{}) +
                   eve::convert(vt_u, eve::as<std::uint32_t>{});
        Vu16 raw = eve::convert(sum, eve::as<std::uint16_t>{});
        set_acc_low(rsp, raw);
        store_vu(dest, raw);
        auto carry = (sum >> 16) != Vu32(0);
        rsp.vco_ref() =
            static_cast<uint16_t>(carry.bitmap().to_ulong() & 0xFFu);
        return true;
    }

    case 0x15: { // VSUBC
        Vu32 s = eve::convert(vs_u, eve::as<std::uint32_t>{});
        Vu32 t = eve::convert(vt_u, eve::as<std::uint32_t>{});
        Vu32 diff = s - t;
        Vu16 raw = eve::convert(diff, eve::as<std::uint16_t>{});
        set_acc_low(rsp, raw);
        store_vu(dest, raw);
        auto borrow = s < t;
        auto noteq = s != t;
        const uint16_t lo =
            static_cast<uint16_t>(borrow.bitmap().to_ulong() & 0xFFu);
        const uint16_t hi =
            static_cast<uint16_t>(noteq.bitmap().to_ulong() & 0xFFu);
        rsp.vco_ref() = static_cast<uint16_t>(lo | (hi << 8));
        return true;
    }

    case 0x20: // VLT
    case 0x21: // VEQ
    case 0x22: // VNE
    case 0x23: // VGE
    {
        const auto vco_lo_nz = as_i16(vco_lo_as_u16(rsp.vco_ref())) != Vi16(0);
        const auto vco_hi_nz = as_i16(vco_hi_as_u16(rsp.vco_ref())) != Vi16(0);
        eve::logical<Vi16> cond;
        switch (funct) {
        case 0x20:
            cond = (vs_i < vt_i) || ((vs_i == vt_i) && vco_lo_nz && vco_hi_nz);
            break;
        case 0x21:
            cond = (vs_i == vt_i) && !vco_hi_nz;
            break;
        case 0x22:
            cond = (vs_i != vt_i) || vco_hi_nz;
            break;
        default:
            cond =
                (vs_i > vt_i) || ((vs_i == vt_i) && (!vco_lo_nz || !vco_hi_nz));
            break;
        }
        Vu16 out = eve::if_else(cond, vs_u, vt_u);
        store_vu(dest, out);
        set_acc_low(rsp, out);
        rsp.vcc_ref() = logical_to_bits(cond);
        rsp.vco_ref() = 0;
        return true;
    }

    case 0x27: { // VMRG
        const Vu16 sel = vcc_lo_as_u16(rsp.vcc_ref());
        Vu16 out = eve::if_else(sel != Vu16(0), vs_u, vt_u);
        store_vu(dest, out);
        set_acc_low(rsp, out);
        rsp.vco_ref() = 0;
        return true;
    }

    case 0x25: { // VCH — CEN64 / parallel-rsp SSE algorithm
        const Vi16 zero(0);
        const auto sign = eve::is_ltz(eve::bit_xor(vs_i, vt_i));
        const Vi16 sign_m = eve::if_else(sign, Vi16(-1), Vi16(0));
        // Conditional negate of vt when signs differ.
        const Vi16 sign_negvt = eve::bit_xor(vt_i, sign_m) - sign_m;
        const Vi16 diff = vs_i - sign_negvt;
        const auto diff_zero = diff == zero;
        const auto vt_neg = vt_i < zero;
        const auto diff_gtz = diff > zero;
        const auto diff_gez = diff_gtz || diff_zero;
        const auto diff_lez = !diff_gtz;
        const auto ge = eve::if_else(sign, vt_neg, diff_gez);
        const auto le = eve::if_else(sign, diff_lez, vt_neg);
        const auto vce = sign && (diff == Vi16(-1));
        const auto neq = !(diff_zero || vce);
        const auto sel = eve::if_else(sign, le, ge);
        const Vi16 result = eve::if_else(sel, sign_negvt, vs_i);
        const Vu16 out = as_u16(result);
        store_vu(dest, out);
        set_acc_low(rsp, out);
        rsp.vcc_ref() = static_cast<uint16_t>(logical_to_bits(le) |
                                              (logical_to_bits(ge) << 8));
        rsp.vco_ref() = static_cast<uint16_t>(logical_to_bits(sign) |
                                              (logical_to_bits(neq) << 8));
        rsp.vce_ref() = static_cast<uint8_t>(logical_to_bits(vce));
        return true;
    }

        // VCL/VCR: keep scalar for now (flag mixing is awkward with eve logical
        // types).

    case 0x1D: { // VSAR — select ACC H/M/L slice into vd
        Vu16 out(0);
        if (element == 8)
            out = load_acc_h(rsp);
        else if (element == 9)
            out = load_acc_m(rsp);
        else if (element == 10)
            out = load_acc_l(rsp);
        store_vu(dest, out);
        return true;
    }

    case 0x33: { // VMOV
        const int de = vs & 7;
        const int se = vmov_src_elem(element, vs);
        dest.set_lane(de, src_t.lane(se));
        set_acc_low(rsp, vt_u);
        return true;
    }

    case 0x30: // VRCP
    case 0x31: // VRCPL
    case 0x32: // VRCPH
    case 0x34: // VRSQ
    case 0x35: // VRSQL
    case 0x36: // VRSQH
    {
        // Single-lane reciprocal with SIMD ACC.L = vt broadcast.
        const int e = element & 7;
        const int de = vs & 7;
        if (funct == 0x32 || funct == 0x36) {
            rsp.divin_ref() = static_cast<int16_t>(src_t.lane(e));
            rsp.divin_loaded_ref() = true;
            dest.set_lane(de, static_cast<uint16_t>(rsp.divout_ref()));
        } else {
            int32_t input;
            if (rsp.divin_loaded_ref() && (funct == 0x31 || funct == 0x35)) {
                input = (static_cast<int32_t>(rsp.divin_ref()) << 16) |
                        src_t.lane(e);
            } else {
                input = static_cast<int16_t>(src_t.lane(e));
            }
            const uint32_t result = (funct == 0x34 || funct == 0x35)
                                        ? rsp_rsq(static_cast<uint32_t>(input))
                                        : rsp_rcp(input);
            rsp.divout_ref() = static_cast<int16_t>((result >> 16) & 0xFFFF);
            rsp.divin_ref() = 0;
            rsp.divin_loaded_ref() = false;
            dest.set_lane(de, static_cast<uint16_t>(result & 0xFFFF));
        }
        set_acc_low(rsp, vt_u);
        return true;
    }

    case 0x28: // VAND
    case 0x29: // VNAND
    case 0x2A: // VOR
    case 0x2B: // VNOR
    case 0x2C: // VXOR
    case 0x2D: // VNXOR
    {
        Vu16 r;
        switch (funct) {
        case 0x28:
            r = eve::bit_and(vs_u, vt_u);
            break;
        case 0x29:
            r = eve::bit_not(eve::bit_and(vs_u, vt_u));
            break;
        case 0x2A:
            r = eve::bit_or(vs_u, vt_u);
            break;
        case 0x2B:
            r = eve::bit_not(eve::bit_or(vs_u, vt_u));
            break;
        case 0x2C:
            r = eve::bit_xor(vs_u, vt_u);
            break;
        default:
            r = eve::bit_not(eve::bit_xor(vs_u, vt_u));
            break;
        }
        store_vu(dest, r);
        set_acc_low(rsp, r);
        return true;
    }

    default:
        return false;
    }
}

} // namespace

void vu_execute_compute_simd(Rsp &rsp, uint32_t inst) {
#if defined(N64_RSP_JIT)
    // Prefer tight SSE2 MAC helpers over eve for the hottest functs.
    {
        const unsigned vd = (inst >> 6) & 0x1F;
        const unsigned vs = (inst >> 11) & 0x1F;
        const unsigned vt = (inst >> 16) & 0x1F;
        const unsigned e = (inst >> 21) & 0xF;
        const unsigned funct = inst & 0x3F;
        if (N64::Rsp::Jit::vu_sse_compute(rsp, vd, vs, vt, e, funct)) {
            vu_profile_compute(inst, true);
            return;
        }
    }
#endif
    const bool hit = try_execute_simd(rsp, inst);
    vu_profile_compute(inst, hit);
    if (!hit)
        vu_execute_compute_scalar(rsp, inst);
}

} // namespace Rsp
} // namespace N64

#endif // N64_RSP_SIMD

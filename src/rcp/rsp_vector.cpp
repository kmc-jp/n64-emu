#include "rcp/rsp.h"
#include "utils/log.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace N64 {
namespace Rsp {

namespace {

// Broadcast lane selection per n64brew element field table.
int broadcast_lane(int element, int dest_lane) {
    if (element < 2)
        return dest_lane;
    if (element < 4)
        return (dest_lane & ~1) | (element & 1);
    if (element < 8)
        return (dest_lane & ~3) | (element & 3);
    return element & 7;
}

int16_t clamp_signed(int64_t accum) {
    if (accum > 32767)
        return 32767;
    if (accum < -32768)
        return -32768;
    return static_cast<int16_t>(accum);
}

uint16_t clamp_unsigned(int64_t accum) {
    if (accum < 0)
        return 0;
    if (accum > 32767)
        return 65535;
    return static_cast<uint16_t>(accum);
}

int64_t sext48(int64_t v) {
    v &= 0xFFFFFFFFFFFFLL;
    if (v & 0x800000000000LL)
        v |= ~0xFFFFFFFFFFFFLL;
    return v;
}

void set_acc(Rsp &rsp, int lane, int64_t v) {
    rsp.acc_set(lane, v);
}

int64_t get_acc(Rsp &rsp, int lane) {
    return rsp.acc_get(lane);
}

} // namespace

void vu_load(Rsp &rsp, uint32_t inst) {
    // LWC2 encoding: base, vt, opcode, element, offset
    // https://n64brew.dev/wiki/Reality_Signal_Processor/CPU_Core
    const int base = (inst >> 21) & 0x1F;
    const int vt = (inst >> 16) & 0x1F;
    const int opcode = (inst >> 11) & 0x1F;
    const int element = (inst >> 7) & 0xF;
    const int offset7 = static_cast<int>(static_cast<int8_t>((inst & 0x7F) << 1) >> 1);

    auto &v = rsp.vreg(vt);
    const uint32_t addr = rsp.gpr(base);

    switch (opcode) {
    case 0x00: { // LBV
        const uint32_t a = addr + offset7;
        v.set_byte(element & 15, rsp.dmem_load8(a));
    } break;
    case 0x01: { // LSV
        const uint32_t a = addr + (offset7 << 1);
        v.set_byte(element & 15, rsp.dmem_load8(a));
        v.set_byte((element + 1) & 15, rsp.dmem_load8(a + 1));
    } break;
    case 0x02: { // LLV
        const uint32_t a = addr + (offset7 << 2);
        for (int i = 0; i < 4; i++)
            v.set_byte((element + i) & 15, rsp.dmem_load8(a + i));
    } break;
    case 0x03: { // LDV
        const uint32_t a = addr + (offset7 << 3);
        for (int i = 0; i < 8; i++)
            v.set_byte((element + i) & 15, rsp.dmem_load8(a + i));
    } break;
    case 0x04: { // LQV
        const uint32_t a = addr + (offset7 << 4);
        const int end = 16 - (static_cast<int>(a) & 0xF);
        for (int i = 0; i < end; i++)
            v.set_byte((element + i) & 15, rsp.dmem_load8(a + i));
    } break;
    case 0x05: { // LRV
        const uint32_t a = addr + (offset7 << 4);
        const int start = 16 - (static_cast<int>(a) & 0xF);
        for (int i = start; i < 16; i++)
            v.set_byte((element + i) & 15,
                       rsp.dmem_load8((a & ~0xFu) + i));
    } break;
    case 0x06: { // LPV
        const uint32_t a = addr + (offset7 << 3);
        for (int i = 0; i < 8; i++) {
            const uint8_t b = rsp.dmem_load8(a + ((element + i) & 15));
            v.set_lane(i, static_cast<uint16_t>(b << 8));
        }
    } break;
    case 0x07: { // LUV
        const uint32_t a = addr + (offset7 << 3);
        for (int i = 0; i < 8; i++) {
            const uint8_t b = rsp.dmem_load8(a + ((element + i) & 15));
            v.set_lane(i, static_cast<uint16_t>(b << 7));
        }
    } break;
    case 0x0B: { // LTV — simplified group load
        const uint32_t a = addr + (offset7 << 4);
        const int vs = vt & ~7;
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 2; j++) {
                rsp.vreg(vs + i).set_byte(
                    ((element / 2) + j) & 15, rsp.dmem_load8(a + i * 2 + j));
            }
        }
    } break;
    default:
        Utils::warn("RSP LWC2 opcode={:#04x}", opcode);
        break;
    }
}

void vu_store(Rsp &rsp, uint32_t inst) {
    const int base = (inst >> 21) & 0x1F;
    const int vt = (inst >> 16) & 0x1F;
    const int opcode = (inst >> 11) & 0x1F;
    const int element = (inst >> 7) & 0xF;
    const int offset7 = static_cast<int>(static_cast<int8_t>((inst & 0x7F) << 1) >> 1);

    auto &v = rsp.vreg(vt);
    const uint32_t addr = rsp.gpr(base);

    switch (opcode) {
    case 0x00: { // SBV
        rsp.dmem_store8(addr + offset7, v.byte(element & 15));
    } break;
    case 0x01: { // SSV
        const uint32_t a = addr + (offset7 << 1);
        rsp.dmem_store8(a, v.byte(element & 15));
        rsp.dmem_store8(a + 1, v.byte((element + 1) & 15));
    } break;
    case 0x02: { // SLV
        const uint32_t a = addr + (offset7 << 2);
        for (int i = 0; i < 4; i++)
            rsp.dmem_store8(a + i, v.byte((element + i) & 15));
    } break;
    case 0x03: { // SDV
        const uint32_t a = addr + (offset7 << 3);
        for (int i = 0; i < 8; i++)
            rsp.dmem_store8(a + i, v.byte((element + i) & 15));
    } break;
    case 0x04: { // SQV
        const uint32_t a = addr + (offset7 << 4);
        const int end = 16 - (static_cast<int>(a) & 0xF);
        for (int i = 0; i < end; i++)
            rsp.dmem_store8(a + i, v.byte((element + i) & 15));
    } break;
    case 0x05: { // SRV
        const uint32_t a = addr + (offset7 << 4);
        const int start = 16 - (static_cast<int>(a) & 0xF);
        for (int i = start; i < 16; i++)
            rsp.dmem_store8((a & ~0xFu) + i, v.byte((element + i) & 15));
    } break;
    case 0x06: { // SPV
        const uint32_t a = addr + (offset7 << 3);
        for (int i = 0; i < 8; i++)
            rsp.dmem_store8(a + i,
                            static_cast<uint8_t>(v.lane(i) >> 8));
    } break;
    case 0x07: { // SUV
        const uint32_t a = addr + (offset7 << 3);
        for (int i = 0; i < 8; i++)
            rsp.dmem_store8(a + i,
                            static_cast<uint8_t>((v.lane(i) >> 7) & 0xFF));
    } break;
    case 0x0A: { // SWV
        const uint32_t a = addr + (offset7 << 4);
        for (int i = 0; i < 16; i++)
            rsp.dmem_store8(a + i, v.byte((element + i) & 15));
    } break;
    case 0x0B: { // STV
        const uint32_t a = addr + (offset7 << 4);
        const int vs = vt & ~7;
        for (int i = 0; i < 8; i++) {
            rsp.dmem_store8(a + i * 2,
                            rsp.vreg(vs + i).byte(
                                (element / 2) & 15));
            rsp.dmem_store8(a + i * 2 + 1,
                            rsp.vreg(vs + i).byte(
                                ((element / 2) + 1) & 15));
        }
    } break;
    default:
        Utils::warn("RSP SWC2 opcode={:#04x}", opcode);
        break;
    }
}

void vu_execute_compute(Rsp &rsp, uint32_t inst) {
    const int vd = (inst >> 6) & 0x1F;
    const int vs = (inst >> 11) & 0x1F;
    const int vt = (inst >> 16) & 0x1F;
    const int element = (inst >> 21) & 0xF;
    const int funct = inst & 0x3F;

    auto &dest = rsp.vreg(vd);
    auto &src_s = rsp.vreg(vs);
    auto &src_t = rsp.vreg(vt);

    auto vt_lane = [&](int lane) -> uint16_t {
        return src_t.lane(broadcast_lane(element, lane));
    };

    switch (funct) {
    case 0x00: // VMULF
    case 0x01: // VMULU
    case 0x08: // VMACF
    case 0x09: // VMACU
    {
        const bool accum = (funct == 0x08 || funct == 0x09);
        const bool unsigned_clamp = (funct == 0x01 || funct == 0x09);
        for (int i = 0; i < 8; i++) {
            const int32_t s = static_cast<int16_t>(src_s.lane(i));
            const int32_t t = static_cast<int16_t>(vt_lane(i));
            int64_t prod = (static_cast<int64_t>(s) * t) << 1;
            if (accum)
                prod += get_acc(rsp, i);
            else if (s == -32768 && t == -32768)
                prod = 0x7FFFFFFFLL << 1; // documented corner
            set_acc(rsp, i, prod);
            const int64_t rounded = get_acc(rsp, i) + 0x8000;
            dest.set_lane(i, unsigned_clamp
                                 ? clamp_unsigned(rounded >> 16)
                                 : static_cast<uint16_t>(clamp_signed(rounded >> 16)));
        }
    } break;

    case 0x04: // VMUDL
    case 0x0C: // VMADL
        for (int i = 0; i < 8; i++) {
            const uint32_t s = src_s.lane(i);
            const uint32_t t = vt_lane(i);
            int64_t prod = (static_cast<int64_t>(s) * t) >> 16;
            if (funct == 0x0C)
                prod += get_acc(rsp, i);
            set_acc(rsp, i, prod);
            dest.set_lane(i, static_cast<uint16_t>(get_acc(rsp, i)));
        }
        break;

    case 0x05: // VMUDM
    case 0x0D: // VMADM
        for (int i = 0; i < 8; i++) {
            const int32_t s = static_cast<int16_t>(src_s.lane(i));
            const uint32_t t = vt_lane(i);
            int64_t prod = static_cast<int64_t>(s) * t;
            if (funct == 0x0D)
                prod += get_acc(rsp, i);
            set_acc(rsp, i, prod);
            dest.set_lane(i, static_cast<uint16_t>(get_acc(rsp, i) >> 16));
        }
        break;

    case 0x06: // VMUDN
    case 0x0E: // VMADN
        for (int i = 0; i < 8; i++) {
            const uint32_t s = src_s.lane(i);
            const int32_t t = static_cast<int16_t>(vt_lane(i));
            int64_t prod = static_cast<int64_t>(s) * t;
            if (funct == 0x0E)
                prod += get_acc(rsp, i);
            set_acc(rsp, i, prod);
            dest.set_lane(i, static_cast<uint16_t>(get_acc(rsp, i)));
        }
        break;

    case 0x07: // VMUDH
    case 0x0F: // VMADH
        for (int i = 0; i < 8; i++) {
            const int32_t s = static_cast<int16_t>(src_s.lane(i));
            const int32_t t = static_cast<int16_t>(vt_lane(i));
            int64_t prod = (static_cast<int64_t>(s) * t) << 16;
            if (funct == 0x0F)
                prod += get_acc(rsp, i);
            set_acc(rsp, i, prod);
            dest.set_lane(i, static_cast<uint16_t>(
                                 clamp_signed(get_acc(rsp, i) >> 16)));
        }
        break;

    case 0x10: // VADD
        for (int i = 0; i < 8; i++) {
            const int32_t s = static_cast<int16_t>(src_s.lane(i));
            const int32_t t = static_cast<int16_t>(vt_lane(i));
            const int32_t sum = s + t + ((rsp.vco_ref() >> i) & 1);
            set_acc(rsp, i, sum);
            dest.set_lane(i, static_cast<uint16_t>(clamp_signed(sum)));
        }
        rsp.vco_ref() = 0;
        break;

    case 0x11: // VSUB
        for (int i = 0; i < 8; i++) {
            const int32_t s = static_cast<int16_t>(src_s.lane(i));
            const int32_t t = static_cast<int16_t>(vt_lane(i));
            const int32_t diff = s - t - ((rsp.vco_ref() >> i) & 1);
            set_acc(rsp, i, diff);
            dest.set_lane(i, static_cast<uint16_t>(clamp_signed(diff)));
        }
        rsp.vco_ref() = 0;
        break;

    case 0x13: // VABS
        for (int i = 0; i < 8; i++) {
            const int16_t s = static_cast<int16_t>(src_s.lane(i));
            const int16_t t = static_cast<int16_t>(vt_lane(i));
            int16_t r;
            if (s < 0) {
                if (t == -32768)
                    r = -32768;
                else
                    r = static_cast<int16_t>(-t);
            } else if (s == 0) {
                r = 0;
            } else {
                r = t;
            }
            set_acc(rsp, i, r);
            dest.set_lane(i, static_cast<uint16_t>(r));
        }
        break;

    case 0x14: // VADDC
        for (int i = 0; i < 8; i++) {
            const uint32_t s = src_s.lane(i);
            const uint32_t t = vt_lane(i);
            const uint32_t sum = s + t;
            set_acc(rsp, i, static_cast<int16_t>(sum));
            dest.set_lane(i, static_cast<uint16_t>(sum));
            if (sum & 0x10000)
                rsp.vco_ref() |= static_cast<uint16_t>(1u << i);
            else
                rsp.vco_ref() &= static_cast<uint16_t>(~(1u << i));
            rsp.vco_ref() &= static_cast<uint16_t>(~(1u << (i + 8)));
        }
        break;

    case 0x15: // VSUBC
        for (int i = 0; i < 8; i++) {
            const uint32_t s = src_s.lane(i);
            const uint32_t t = vt_lane(i);
            const uint32_t diff = s - t;
            set_acc(rsp, i, static_cast<int16_t>(diff));
            dest.set_lane(i, static_cast<uint16_t>(diff));
            const bool noteq = (s != t);
            const bool borrow = (s < t);
            if (borrow)
                rsp.vco_ref() |= static_cast<uint16_t>(1u << i);
            else
                rsp.vco_ref() &= static_cast<uint16_t>(~(1u << i));
            if (noteq)
                rsp.vco_ref() |= static_cast<uint16_t>(1u << (i + 8));
            else
                rsp.vco_ref() &= static_cast<uint16_t>(~(1u << (i + 8)));
        }
        break;

    case 0x1D: { // VSAR
        const int e = element;
        for (int i = 0; i < 8; i++) {
            const int64_t a = get_acc(rsp, i);
            uint16_t out = 0;
            if (e == 8)
                out = static_cast<uint16_t>(a >> 32);
            else if (e == 9)
                out = static_cast<uint16_t>(a >> 16);
            else if (e == 10)
                out = static_cast<uint16_t>(a);
            dest.set_lane(i, out);
        }
    } break;

    case 0x20: // VLT
    case 0x21: // VEQ
    case 0x22: // VNE
    case 0x23: // VGE
        for (int i = 0; i < 8; i++) {
            const int16_t s = static_cast<int16_t>(src_s.lane(i));
            const int16_t t = static_cast<int16_t>(vt_lane(i));
            bool cond = false;
            const bool vco_lo = (rsp.vco_ref() >> i) & 1;
            const bool vco_hi = (rsp.vco_ref() >> (i + 8)) & 1;
            switch (funct) {
            case 0x20:
                cond = (s < t) || (s == t && vco_lo && vco_hi);
                break;
            case 0x21:
                cond = (s == t) && !vco_hi;
                break;
            case 0x22:
                cond = (s != t) || vco_hi;
                break;
            case 0x23:
                cond = (s > t) || (s == t && (!vco_lo || !vco_hi));
                break;
            }
            if (cond)
                rsp.vcc_ref() |= static_cast<uint16_t>(1u << i);
            else
                rsp.vcc_ref() &= static_cast<uint16_t>(~(1u << i));
            dest.set_lane(i, cond ? src_s.lane(i) : vt_lane(i));
            set_acc(rsp, i, static_cast<int16_t>(dest.lane(i)));
        }
        rsp.vcc_ref() &= 0xFF;
        rsp.vco_ref() = 0;
        break;

    case 0x24: // VCL
    case 0x25: // VCH
    case 0x26: // VCR — approximate using compare/select
        for (int i = 0; i < 8; i++) {
            const int16_t s = static_cast<int16_t>(src_s.lane(i));
            const int16_t t = static_cast<int16_t>(vt_lane(i));
            // Simplified: geq select for bootstrapping; refine later if needed.
            const bool ge = s >= t;
            dest.set_lane(i, ge ? static_cast<uint16_t>(s)
                                : static_cast<uint16_t>(t));
            set_acc(rsp, i, static_cast<int16_t>(dest.lane(i)));
            if (ge)
                rsp.vcc_ref() |= static_cast<uint16_t>(1u << i);
            else
                rsp.vcc_ref() &= static_cast<uint16_t>(~(1u << i));
        }
        rsp.vco_ref() = 0;
        break;

    case 0x27: // VMRG
        for (int i = 0; i < 8; i++) {
            const bool sel = (rsp.vcc_ref() >> i) & 1;
            dest.set_lane(i, sel ? src_s.lane(i) : vt_lane(i));
            set_acc(rsp, i, static_cast<int16_t>(dest.lane(i)));
        }
        rsp.vco_ref() = 0;
        break;

    case 0x28: // VAND
    case 0x29: // VNAND
    case 0x2A: // VOR
    case 0x2B: // VNOR
    case 0x2C: // VXOR
    case 0x2D: // VNXOR
        for (int i = 0; i < 8; i++) {
            uint16_t r = 0;
            const uint16_t s = src_s.lane(i);
            const uint16_t t = vt_lane(i);
            switch (funct) {
            case 0x28:
                r = s & t;
                break;
            case 0x29:
                r = static_cast<uint16_t>(~(s & t));
                break;
            case 0x2A:
                r = s | t;
                break;
            case 0x2B:
                r = static_cast<uint16_t>(~(s | t));
                break;
            case 0x2C:
                r = s ^ t;
                break;
            case 0x2D:
                r = static_cast<uint16_t>(~(s ^ t));
                break;
            }
            dest.set_lane(i, r);
            set_acc(rsp, i, static_cast<int16_t>(r));
        }
        break;

    case 0x30: // VRCP
    case 0x31: // VRCPL
    case 0x32: // VRCPH
    case 0x34: // VRSQ
    case 0x35: // VRSQL
    case 0x36: // VRSQH
    {
        // Reciprocal / rsqrt estimate — enough for many microcodes.
        const int se = element & 7;
        int16_t input;
        if (funct == 0x32 || funct == 0x36) { // high
            input = static_cast<int16_t>(vt_lane(se));
            rsp.divin_ref() = input;
            rsp.divin_loaded_ref() = true;
            dest.set_lane(vd & 7, static_cast<uint16_t>(rsp.divout_ref()));
        } else {
            input = static_cast<int16_t>(vt_lane(se));
            if (rsp.divin_loaded_ref() && (funct == 0x31 || funct == 0x35)) {
                const int32_t full =
                    (static_cast<int32_t>(rsp.divin_ref()) << 16) |
                    static_cast<uint16_t>(input);
                double d = full;
                double r = (funct == 0x34 || funct == 0x35)
                               ? (d != 0 ? 1.0 / std::sqrt(std::fabs(d)) : 0)
                               : (d != 0 ? 1.0 / d : 0);
                rsp.divout_ref() = static_cast<int16_t>(r * 65536.0);
                rsp.divin_loaded_ref() = false;
            } else {
                double d = input;
                double r = (funct == 0x34 || funct == 0x35)
                               ? (d != 0 ? 1.0 / std::sqrt(std::fabs(d)) : 0)
                               : (d != 0 ? 1.0 / d : 0);
                rsp.divout_ref() = static_cast<int16_t>(r * 32768.0);
            }
            for (int i = 0; i < 8; i++)
                set_acc(rsp, i, static_cast<int16_t>(src_s.lane(i)));
            dest.set_lane(se, static_cast<uint16_t>(rsp.divout_ref()));
        }
        (void)vs;
    } break;

    case 0x33: // VMOV
        for (int i = 0; i < 8; i++) {
            const uint16_t t = vt_lane(element < 8 ? i : (element & 7));
            // VMOV moves single element; element selects source lane into all?
            dest.set_lane(i, src_t.lane(broadcast_lane(element, i)));
            set_acc(rsp, i, static_cast<int16_t>(dest.lane(i)));
            (void)t;
        }
        if (element >= 8) {
            const int e = element & 7;
            dest.set_lane(e, src_t.lane(e));
        }
        break;

    case 0x37: // VNOP
        break;

    default:
        Utils::warn("RSP VU funct={:#04x}", funct);
        break;
    }
}

} // namespace Rsp
} // namespace N64

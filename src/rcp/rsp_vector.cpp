#include "rcp/rsp.h"
#include "rcp/rsp_rom.h"
#include "utils/log.h"
#include <algorithm>
#include <array>
#include <climits>
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

void set_vcc_bit(Rsp &rsp, int i, bool lo, bool val) {
    const int bit = lo ? i : (i + 8);
    if (val)
        rsp.vcc_ref() |= static_cast<uint16_t>(1u << bit);
    else
        rsp.vcc_ref() &= static_cast<uint16_t>(~(1u << bit));
}

void set_vco_bit(Rsp &rsp, int i, bool lo, bool val) {
    const int bit = lo ? i : (i + 8);
    if (val)
        rsp.vco_ref() |= static_cast<uint16_t>(1u << bit);
    else
        rsp.vco_ref() &= static_cast<uint16_t>(~(1u << bit));
}

void set_vce_bit(Rsp &rsp, int i, bool val) {
    if (val)
        rsp.vce_ref() |= static_cast<uint8_t>(1u << i);
    else
        rsp.vce_ref() &= static_cast<uint8_t>(~(1u << i));
}

bool vcc_bit(Rsp &rsp, int i, bool lo) {
    const int bit = lo ? i : (i + 8);
    return (rsp.vcc_ref() >> bit) & 1;
}

bool vco_bit(Rsp &rsp, int i, bool lo) {
    const int bit = lo ? i : (i + 8);
    return (rsp.vco_ref() >> bit) & 1;
}

bool vce_bit(Rsp &rsp, int i) {
    return (rsp.vce_ref() >> i) & 1;
}

void set_acc_l(Rsp &rsp, int lane, uint16_t v) {
    rsp.acc_l().set_lane(lane, v);
}

uint32_t rsp_rcp(int32_t sinput) {
    const int32_t mask = sinput >> 31;
    int32_t input = sinput ^ mask;
    if (sinput > INT16_MIN)
        input -= mask;
    if (input == 0)
        return 0x7FFFFFFF;
    if (sinput == INT16_MIN)
        return 0xFFFF0000;
    const uint32_t shift = static_cast<uint32_t>(__builtin_clz(static_cast<uint32_t>(input)));
    const uint32_t index =
        static_cast<uint32_t>((((static_cast<uint64_t>(input) << shift) & 0x7FC00000ULL) >> 22));
    int32_t result = kRcpRom[index];
    result = (0x10000 | result) << 14;
    result = (result >> (31 - static_cast<int>(shift))) ^ mask;
    return static_cast<uint32_t>(result);
}

uint32_t rsp_rsq(uint32_t input) {
    if (input == 0)
        return 0x7FFFFFFF;
    if (input == 0xFFFF8000)
        return 0xFFFF0000;
    if (input > 0xFFFF8000)
        input--;
    int32_t sinput = static_cast<int32_t>(input);
    const int32_t mask = sinput >> 31;
    input ^= static_cast<uint32_t>(mask);
    const int shift = __builtin_clz(input) + 1;
    const int index =
        static_cast<int>(((input << shift) >> 24) | ((shift & 1) << 8));
    const uint32_t rom = (static_cast<uint32_t>(kRsqRom[index]) << 14);
    const int r_shift = (32 - shift) >> 1;
    uint32_t result = (0x40000000u | rom) >> r_shift;
    return result ^ static_cast<uint32_t>(mask);
}

int vmov_src_elem(int element, int vs_field) {
    if (element <= 1)
        return (element & 0b000) | (vs_field & 0b111);
    if (element <= 3)
        return (element & 0b001) | (vs_field & 0b110);
    if (element <= 7)
        return (element & 0b011) | (vs_field & 0b100);
    return (element & 0b111);
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

// Low-slice write for VMUDL/VMADL/VMUDN/VMADN: if ACC high::mid is a
// sign-extension of ACC.mid, return ACC.low; else saturate.
uint16_t clamp_acc_low(int64_t acc) {
    const int16_t hi = static_cast<int16_t>(acc >> 32);
    const int16_t mid = static_cast<int16_t>(acc >> 16);
    const uint16_t lo = static_cast<uint16_t>(acc);
    if (hi == 0) {
        if ((mid & 0x8000) == 0)
            return lo;
    } else if (hi == -1) {
        if ((mid & 0x8000) == 0x8000)
            return lo;
    }
    return (hi < 0) ? 0 : 0xFFFF;
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
        // Second byte is dropped (not wrapped) when element == 15.
        if (element < 15)
            v.set_byte(element + 1, rsp.dmem_load8(a + 1));
    } break;
    case 0x02: { // LLV
        const uint32_t a = addr + (offset7 << 2);
        for (int i = 0; i < 4; i++) {
            if (element + i > 15)
                break;
            v.set_byte(element + i, rsp.dmem_load8(a + i));
        }
    } break;
    case 0x03: { // LDV
        const uint32_t a = addr + (offset7 << 3);
        const int end = std::min(element + 8, 16);
        for (int i = element; i < end; i++)
            v.set_byte(i, rsp.dmem_load8(a + (i - element)));
    } break;
    case 0x04: { // LQV
        const uint32_t a = addr + (offset7 << 4);
        const uint32_t end_a = (a & ~15u) + 15u;
        for (int i = 0; a + static_cast<uint32_t>(i) <= end_a &&
                        i + element < 16;
             i++)
            v.set_byte(element + i, rsp.dmem_load8(a + i));
    } break;
    case 0x05: { // LRV
        uint32_t a = addr + (offset7 << 4);
        int start = 16 - ((static_cast<int>(a) & 0xF) - element);
        a &= ~15u;
        for (int i = start; i < 16; i++)
            v.set_byte(i & 15, rsp.dmem_load8(a++));
    } break;
    case 0x06: { // LPV
        uint32_t a = addr + (offset7 << 3);
        const int addr_ofs = static_cast<int>(a & 7);
        a &= ~7u;
        for (int i = 0; i < 8; i++) {
            const int eo = (16 - element + (i + addr_ofs)) & 0xF;
            const uint8_t b = rsp.dmem_load8(a + eo);
            v.set_lane(i, static_cast<uint16_t>(b << 8));
        }
    } break;
    case 0x07: { // LUV
        uint32_t a = addr + (offset7 << 3);
        const int addr_ofs = static_cast<int>(a & 7);
        a &= ~7u;
        for (int i = 0; i < 8; i++) {
            const int eo = (16 - element + (i + addr_ofs)) & 0xF;
            const uint8_t b = rsp.dmem_load8(a + eo);
            v.set_lane(i, static_cast<uint16_t>(b << 7));
        }
    } break;
    case 0x08: { // LHV
        uint32_t a = addr + (offset7 << 4);
        const int addr_ofs = static_cast<int>(a & 7);
        a &= ~7u;
        for (int i = 0; i < 8; i++) {
            const int ofs = ((16 - element) + (i * 2) + addr_ofs) & 0xF;
            const uint8_t b = rsp.dmem_load8(a + ofs);
            v.set_lane(i, static_cast<uint16_t>(b << 7));
        }
    } break;
    case 0x09: { // LFV
        uint32_t a = addr + (offset7 << 4);
        int base_ofs = static_cast<int>(a & 7) - element;
        a &= ~7u;
        const int start = element;
        const int end = std::min(start + 8, 16);
        VuReg tmp{};
        for (int offset = 0; offset < 4; offset++) {
            const uint8_t b0 =
                rsp.dmem_load8(a + ((base_ofs + offset * 4 + 0) & 15));
            const uint8_t b1 =
                rsp.dmem_load8(a + ((base_ofs + offset * 4 + 8) & 15));
            tmp.set_lane(offset, static_cast<uint16_t>(b0 << 7));
            tmp.set_lane(offset + 4, static_cast<uint16_t>(b1 << 7));
        }
        for (int i = start; i < end; i++)
            v.set_byte(i, tmp.byte(i));
    } break;
    case 0x0B: { // LTV
        uint32_t base = (addr + (offset7 << 4)) & ~7u;
        for (int i = 0; i < 8; i++) {
            const uint32_t offset =
                static_cast<uint32_t>((i * 2) + element + (base & 8));
            const uint16_t hi = rsp.dmem_load8(base + ((offset + 0) & 0xF));
            const uint16_t lo = rsp.dmem_load8(base + ((offset + 1) & 0xF));
            const int reg = (vt & 0x18) | ((i + (element >> 1)) & 0x7);
            rsp.vreg(reg).set_lane(i & 7, static_cast<uint16_t>((hi << 8) | lo));
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
        const uint32_t end_a = (a & ~15u) + 15u;
        for (int i = 0; a + static_cast<uint32_t>(i) <= end_a; i++)
            rsp.dmem_store8(a + i, v.byte((element + i) & 15));
    } break;
    case 0x05: { // SRV
        uint32_t a = addr + (offset7 << 4);
        const int start = element;
        const int end = start + static_cast<int>(a & 15);
        const int base_ofs = 16 - static_cast<int>(a & 15);
        a &= ~15u;
        for (int i = start; i < end; i++)
            rsp.dmem_store8(a++, v.byte((i + base_ofs) & 15));
    } break;
    case 0x06: { // SPV
        uint32_t a = addr + (offset7 << 3);
        const int start = element;
        for (int offset = start; offset < start + 8; offset++) {
            if ((offset & 15) < 8) {
                rsp.dmem_store8(a++, v.byte((offset & 7) << 1));
            } else {
                rsp.dmem_store8(
                    a++,
                    static_cast<uint8_t>((v.lane(offset & 7) >> 7) & 0xFF));
            }
        }
    } break;
    case 0x07: { // SUV
        uint32_t a = addr + (offset7 << 3);
        const int start = element;
        for (int offset = start; offset < start + 8; offset++) {
            if ((offset & 15) < 8) {
                rsp.dmem_store8(
                    a++,
                    static_cast<uint8_t>((v.lane(offset & 7) >> 7) & 0xFF));
            } else {
                rsp.dmem_store8(a++, v.byte((offset & 7) << 1));
            }
        }
    } break;
    case 0x08: { // SHV
        uint32_t a = addr + (offset7 << 4);
        const int in_ofs = static_cast<int>(a & 7);
        a &= ~7u;
        for (int i = 0; i < 8; i++) {
            const int byte_index = (i * 2) + element;
            uint16_t val = static_cast<uint16_t>(v.byte(byte_index & 15) << 1);
            val |= static_cast<uint16_t>(v.byte((byte_index + 1) & 15) >> 7);
            const int ofs = in_ofs + (i * 2);
            rsp.dmem_store8(a + (ofs & 0xF), static_cast<uint8_t>(val));
        }
    } break;
    case 0x09: { // SFV
        uint32_t a = addr + (offset7 << 4);
        const int base_ofs = static_cast<int>(a & 7);
        a &= ~7u;
        uint8_t values[4] = {0, 0, 0, 0};
        switch (element) {
        case 0:
        case 15:
            values[0] = static_cast<uint8_t>(v.lane(0) >> 7);
            values[1] = static_cast<uint8_t>(v.lane(1) >> 7);
            values[2] = static_cast<uint8_t>(v.lane(2) >> 7);
            values[3] = static_cast<uint8_t>(v.lane(3) >> 7);
            break;
        case 1:
            values[0] = static_cast<uint8_t>(v.lane(6) >> 7);
            values[1] = static_cast<uint8_t>(v.lane(7) >> 7);
            values[2] = static_cast<uint8_t>(v.lane(4) >> 7);
            values[3] = static_cast<uint8_t>(v.lane(5) >> 7);
            break;
        case 4:
            values[0] = static_cast<uint8_t>(v.lane(1) >> 7);
            values[1] = static_cast<uint8_t>(v.lane(2) >> 7);
            values[2] = static_cast<uint8_t>(v.lane(3) >> 7);
            values[3] = static_cast<uint8_t>(v.lane(0) >> 7);
            break;
        case 5:
            values[0] = static_cast<uint8_t>(v.lane(7) >> 7);
            values[1] = static_cast<uint8_t>(v.lane(4) >> 7);
            values[2] = static_cast<uint8_t>(v.lane(5) >> 7);
            values[3] = static_cast<uint8_t>(v.lane(6) >> 7);
            break;
        case 8:
            values[0] = static_cast<uint8_t>(v.lane(4) >> 7);
            values[1] = static_cast<uint8_t>(v.lane(5) >> 7);
            values[2] = static_cast<uint8_t>(v.lane(6) >> 7);
            values[3] = static_cast<uint8_t>(v.lane(7) >> 7);
            break;
        case 11:
            values[0] = static_cast<uint8_t>(v.lane(3) >> 7);
            values[1] = static_cast<uint8_t>(v.lane(0) >> 7);
            values[2] = static_cast<uint8_t>(v.lane(1) >> 7);
            values[3] = static_cast<uint8_t>(v.lane(2) >> 7);
            break;
        case 12:
            values[0] = static_cast<uint8_t>(v.lane(5) >> 7);
            values[1] = static_cast<uint8_t>(v.lane(6) >> 7);
            values[2] = static_cast<uint8_t>(v.lane(7) >> 7);
            values[3] = static_cast<uint8_t>(v.lane(4) >> 7);
            break;
        default:
            break;
        }
        for (int i = 0; i < 4; i++)
            rsp.dmem_store8(a + ((base_ofs + (i << 2)) & 15), values[i]);
    } break;
    case 0x0A: { // SWV
        uint32_t a = addr + (offset7 << 4);
        int base_ofs = static_cast<int>(a & 7);
        a &= ~7u;
        for (int i = element; i < element + 16; i++) {
            rsp.dmem_store8(a + (base_ofs & 15), v.byte(i & 15));
            base_ofs++;
        }
    } break;
    case 0x0B: { // STV
        uint32_t base = addr + (offset7 << 4);
        const uint32_t in_ofs = base & 7;
        base &= ~7u;
        const int e = element >> 1;
        for (int i = 0; i < 8; i++) {
            const uint32_t offset = static_cast<uint32_t>(i * 2) + in_ofs;
            const int reg = (vt & 0x18) | ((i + e) & 0x7);
            const uint16_t val = rsp.vreg(reg).lane(i & 7);
            rsp.dmem_store8(base + ((offset + 0) & 0xF),
                            static_cast<uint8_t>(val >> 8));
            rsp.dmem_store8(base + ((offset + 1) & 0xF),
                            static_cast<uint8_t>(val & 0xFF));
        }
    } break;
    default:
        Utils::warn("RSP SWC2 opcode={:#04x}", opcode);
        break;
    }
}

void vu_execute_compute_scalar(Rsp &rsp, uint32_t inst) {
    const int vd = (inst >> 6) & 0x1F;
    const int vs = (inst >> 11) & 0x1F;
    const int vt = (inst >> 16) & 0x1F;
    const int element = (inst >> 21) & 0xF;
    const int funct = inst & 0x3F;

    auto &dest = rsp.vreg(vd);
    // Snapshot VS/VT so vd aliasing either source cannot change later lanes
    // mid-instruction (hardware reads operands before writing vd).
    const VuReg src_s = rsp.vreg(vs);
    const VuReg src_t = rsp.vreg(vt);

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
            // s16*s16 product then <<1; VMUL* also adds round bias into ACC.
            int64_t prod = (static_cast<int64_t>(s) * t) << 1;
            if (accum) {
                set_acc(rsp, i, get_acc(rsp, i) + prod);
            } else {
                set_acc(rsp, i, prod + 0x8000);
            }
            const int64_t out = get_acc(rsp, i);
            dest.set_lane(i, unsigned_clamp
                                 ? clamp_unsigned(out >> 16)
                                 : static_cast<uint16_t>(clamp_signed(out >> 16)));
        }
    } break;

    case 0x02: // VRNDP
        for (int i = 0; i < 8; i++) {
            int32_t product = static_cast<int16_t>(vt_lane(i));
            if (vs & 1)
                product <<= 16;
            int64_t acc = get_acc(rsp, i);
            if (acc >= 0)
                acc += product;
            set_acc(rsp, i, acc);
            dest.set_lane(i, static_cast<uint16_t>(
                                 clamp_signed(get_acc(rsp, i) >> 16)));
        }
        break;

    case 0x03: // VMULQ
        for (int i = 0; i < 8; i++) {
            int32_t product = static_cast<int16_t>(src_s.lane(i)) *
                             static_cast<int16_t>(vt_lane(i));
            if (product < 0)
                product += 31;
            // ACC = product in H:M, L=0; result = clamp(product>>1) & ~15
            const int64_t acc = (static_cast<int64_t>(product) << 16) &
                                ~0xFFFFLL;
            set_acc(rsp, i, acc);
            dest.set_lane(i, static_cast<uint16_t>(
                                 clamp_signed(product >> 1) & ~15));
        }
        break;

    case 0x0A: // VRNDN
        for (int i = 0; i < 8; i++) {
            int32_t product = static_cast<int16_t>(vt_lane(i));
            if (vs & 1)
                product <<= 16;
            int64_t acc = get_acc(rsp, i);
            if (acc < 0)
                acc += product;
            set_acc(rsp, i, acc);
            dest.set_lane(i, static_cast<uint16_t>(
                                 clamp_signed(get_acc(rsp, i) >> 16)));
        }
        break;

    case 0x0B: // VMACQ
        for (int i = 0; i < 8; i++) {
            int32_t product =
                static_cast<int32_t>(get_acc(rsp, i) >> 16);
            if (product < 0 && !(product & (1 << 5)))
                product += 32;
            else if (product >= 32 && !(product & (1 << 5)))
                product -= 32;
            const int64_t lo = get_acc(rsp, i) & 0xFFFF;
            set_acc(rsp, i, (static_cast<int64_t>(product) << 16) | lo);
            dest.set_lane(i, static_cast<uint16_t>(
                                 clamp_signed(product >> 1) & ~15));
        }
        break;

    case 0x04: // VMUDL
    case 0x0C: // VMADL
        for (int i = 0; i < 8; i++) {
            const uint32_t s = src_s.lane(i);
            const uint32_t t = vt_lane(i);
            int64_t prod = (static_cast<int64_t>(s) * t) >> 16;
            if (funct == 0x0C)
                prod += get_acc(rsp, i);
            set_acc(rsp, i, prod);
            dest.set_lane(i, clamp_acc_low(get_acc(rsp, i)));
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
            dest.set_lane(i, static_cast<uint16_t>(
                                 clamp_signed(get_acc(rsp, i) >> 16)));
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
            dest.set_lane(i, clamp_acc_low(get_acc(rsp, i)));
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
            set_acc_l(rsp, i, static_cast<uint16_t>(sum));
            dest.set_lane(i, static_cast<uint16_t>(clamp_signed(sum)));
        }
        rsp.vco_ref() = 0;
        break;

    case 0x11: // VSUB
        for (int i = 0; i < 8; i++) {
            const int32_t s = static_cast<int16_t>(src_s.lane(i));
            const int32_t t = static_cast<int16_t>(vt_lane(i));
            const int32_t diff = s - t - ((rsp.vco_ref() >> i) & 1);
            set_acc_l(rsp, i, static_cast<uint16_t>(diff));
            dest.set_lane(i, static_cast<uint16_t>(clamp_signed(diff)));
        }
        rsp.vco_ref() = 0;
        break;

    case 0x13: // VABS
        for (int i = 0; i < 8; i++) {
            const int16_t s = static_cast<int16_t>(src_s.lane(i));
            const uint16_t t = vt_lane(i);
            if (s < 0) {
                if (t == 0x8000) {
                    dest.set_lane(i, 0x7FFF);
                    set_acc_l(rsp, i, 0x8000);
                } else {
                    const uint16_t r = static_cast<uint16_t>(-static_cast<int16_t>(t));
                    dest.set_lane(i, r);
                    set_acc_l(rsp, i, r);
                }
            } else if (s == 0) {
                dest.set_lane(i, 0);
                set_acc_l(rsp, i, 0);
            } else {
                dest.set_lane(i, t);
                set_acc_l(rsp, i, t);
            }
        }
        break;

    case 0x14: // VADDC
        for (int i = 0; i < 8; i++) {
            const uint32_t s = src_s.lane(i);
            const uint32_t t = vt_lane(i);
            const uint32_t sum = s + t;
            set_acc_l(rsp, i, static_cast<uint16_t>(sum));
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
            set_acc_l(rsp, i, static_cast<uint16_t>(diff));
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
            set_acc_l(rsp, i, dest.lane(i));
        }
        rsp.vcc_ref() &= 0xFF;
        rsp.vco_ref() = 0;
        break;

    case 0x25: { // VCH
        for (int i = 0; i < 8; i++) {
            const int16_t s = static_cast<int16_t>(src_s.lane(i));
            const int16_t t = static_cast<int16_t>(vt_lane(i));
            if ((s ^ t) < 0) {
                const int16_t result =
                    static_cast<int16_t>(s + t);
                const uint16_t acc =
                    (result <= 0) ? static_cast<uint16_t>(-t)
                                  : static_cast<uint16_t>(s);
                set_acc_l(rsp, i, acc);
                set_vcc_bit(rsp, i, true, result <= 0);
                set_vcc_bit(rsp, i, false, t < 0);
                set_vco_bit(rsp, i, true, true);
                set_vco_bit(rsp, i, false,
                            result != 0 &&
                                static_cast<uint16_t>(s) !=
                                    (static_cast<uint16_t>(t) ^ 0xFFFF));
                set_vce_bit(rsp, i, result == -1);
            } else {
                const int16_t result =
                    static_cast<int16_t>(s - t);
                const uint16_t acc =
                    (result >= 0) ? static_cast<uint16_t>(t)
                                  : static_cast<uint16_t>(s);
                set_acc_l(rsp, i, acc);
                set_vcc_bit(rsp, i, true, t < 0);
                set_vcc_bit(rsp, i, false, result >= 0);
                set_vco_bit(rsp, i, true, false);
                set_vco_bit(rsp, i, false,
                            result != 0 &&
                                static_cast<uint16_t>(s) !=
                                    (static_cast<uint16_t>(t) ^ 0xFFFF));
                set_vce_bit(rsp, i, false);
            }
            dest.set_lane(i, static_cast<uint16_t>(rsp.acc_get(i) & 0xFFFF));
        }
        break;
    }

    case 0x24: { // VCL
        for (int i = 0; i < 8; i++) {
            const uint16_t s = src_s.lane(i);
            const uint16_t t = vt_lane(i);
            if (vco_bit(rsp, i, true)) {
                if (vco_bit(rsp, i, false)) {
                    set_acc_l(rsp, i,
                              vcc_bit(rsp, i, true) ? static_cast<uint16_t>(-t)
                                                    : s);
                } else {
                    const uint32_t sum = static_cast<uint32_t>(s) + t;
                    const uint16_t clamped = static_cast<uint16_t>(sum);
                    const bool overflow = sum != clamped;
                    if (vce_bit(rsp, i)) {
                        set_vcc_bit(rsp, i, true, !clamped || !overflow);
                        set_acc_l(rsp, i,
                                  vcc_bit(rsp, i, true)
                                      ? static_cast<uint16_t>(-t)
                                      : s);
                    } else {
                        set_vcc_bit(rsp, i, true, !clamped && !overflow);
                        set_acc_l(rsp, i,
                                  vcc_bit(rsp, i, true)
                                      ? static_cast<uint16_t>(-t)
                                      : s);
                    }
                }
            } else {
                if (vco_bit(rsp, i, false)) {
                    set_acc_l(rsp, i,
                              vcc_bit(rsp, i, false) ? t : s);
                } else {
                    set_vcc_bit(rsp, i, false,
                                static_cast<int32_t>(s) -
                                        static_cast<int32_t>(t) >=
                                    0);
                    set_acc_l(rsp, i,
                              vcc_bit(rsp, i, false) ? t : s);
                }
            }
            dest.set_lane(i, static_cast<uint16_t>(rsp.acc_get(i) & 0xFFFF));
        }
        rsp.vco_ref() = 0;
        rsp.vce_ref() = 0;
        break;
    }

    case 0x26: { // VCR
        for (int i = 0; i < 8; i++) {
            const uint16_t s = src_s.lane(i);
            const uint16_t t = vt_lane(i);
            const bool sign_different =
                ((0x8000 & (s ^ t)) == 0x8000);
            const uint16_t vt_abs =
                sign_different ? static_cast<uint16_t>(~t) : t;
            const bool gte =
                static_cast<int16_t>(t) <=
                static_cast<int16_t>(sign_different ? 0xFFFF : s);
            const bool lte =
                ((((sign_different ? s : 0) + t) & 0x8000) == 0x8000);
            const bool check = sign_different ? lte : gte;
            const uint16_t result = check ? vt_abs : s;
            set_acc_l(rsp, i, result);
            dest.set_lane(i, result);
            set_vcc_bit(rsp, i, true, lte);
            set_vcc_bit(rsp, i, false, gte);
        }
        rsp.vco_ref() = 0;
        rsp.vce_ref() = 0;
        break;
    }

    case 0x27: // VMRG
        for (int i = 0; i < 8; i++) {
            const bool sel = (rsp.vcc_ref() >> i) & 1;
            dest.set_lane(i, sel ? src_s.lane(i) : vt_lane(i));
            set_acc_l(rsp, i, dest.lane(i));
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
            set_acc_l(rsp, i, r);
        }
        break;

    case 0x30: // VRCP
    case 0x31: // VRCPL
    case 0x32: // VRCPH
    case 0x34: // VRSQ
    case 0x35: // VRSQL
    case 0x36: // VRSQH
    {
        const int e = element & 7;
        const int de = vs & 7; // dest element encoded in VS field
        if (funct == 0x32 || funct == 0x36) { // high: latch divin
            rsp.divin_ref() = static_cast<int16_t>(src_t.lane(e));
            rsp.divin_loaded_ref() = true;
            dest.set_lane(de, static_cast<uint16_t>(rsp.divout_ref()));
            for (int i = 0; i < 8; i++)
                set_acc_l(rsp, i, vt_lane(i));
        } else {
            int32_t input;
            if (rsp.divin_loaded_ref() && (funct == 0x31 || funct == 0x35)) {
                input = (static_cast<int32_t>(rsp.divin_ref()) << 16) |
                        src_t.lane(e);
            } else {
                input = static_cast<int16_t>(src_t.lane(e));
            }
            const uint32_t result =
                (funct == 0x34 || funct == 0x35)
                    ? rsp_rsq(static_cast<uint32_t>(input))
                    : rsp_rcp(input);
            rsp.divout_ref() =
                static_cast<int16_t>((result >> 16) & 0xFFFF);
            rsp.divin_ref() = 0;
            rsp.divin_loaded_ref() = false;
            dest.set_lane(de, static_cast<uint16_t>(result & 0xFFFF));
            for (int i = 0; i < 8; i++)
                set_acc_l(rsp, i, vt_lane(i));
        }
    } break;

    case 0x33: { // VMOV — VS field is dest element, not a vector register
        const int de = vs & 7;
        const int se = vmov_src_elem(element, vs);
        const uint16_t vte_elem = src_t.lane(se);
        dest.set_lane(de, vte_elem);
        for (int i = 0; i < 8; i++)
            set_acc_l(rsp, i, vt_lane(i));
    } break;

    case 0x37: // VNOP
        break;

    // Undocumented / reserved opcodes behave like VZERO on real RSP.
    case 0x12: // VSUT
    case 0x16: // VADDB
    case 0x17: // VSUBB
    case 0x18: // VACCB
    case 0x19: // VSUCB
    case 0x1A: // VSAD
    case 0x1B: // VSAC
    case 0x1C: // VSUM
    case 0x1E:
    case 0x1F:
    case 0x2E:
    case 0x2F:
    case 0x38: // VEXTT
    case 0x39: // VEXTQ
    case 0x3A: // VEXTN
    case 0x3B:
    case 0x3C: // VINST
    case 0x3D: // VINSQ
    case 0x3E: // VINSN
    case 0x3F:
        for (int i = 0; i < 8; i++) {
            set_acc_l(rsp, i,
                      static_cast<uint16_t>(src_s.lane(i) + vt_lane(i)));
            dest.set_lane(i, 0);
        }
        break;

    default:
        Utils::warn("RSP VU funct={:#04x}", funct);
        break;
    }
}

void vu_execute_compute(Rsp &rsp, uint32_t inst) {
#if N64_RSP_SIMD
    vu_execute_compute_simd(rsp, inst);
#else
    vu_execute_compute_scalar(rsp, inst);
#endif
}

} // namespace Rsp
} // namespace N64

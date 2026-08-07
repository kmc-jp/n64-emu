#include "fpu_instruction_impl.h"
#include "cpu/cpu.h"
#include "memory/bus.h"
#include "mmu/mmu.h"
#include "mmu/tlb.h"
#include "utils/log.h"
#include <cmath>
#include <cstring>
#include <optional>

namespace N64::Cpu {

namespace {

float bits_to_float(uint32_t bits) {
    float value;
    static_assert(sizeof(float) == sizeof(uint32_t));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t float_to_bits(float value) {
    uint32_t bits;
    static_assert(sizeof(float) == sizeof(uint32_t));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

double bits_to_double(uint64_t bits) {
    double value;
    static_assert(sizeof(double) == sizeof(uint64_t));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint64_t double_to_bits(double value) {
    uint64_t bits;
    static_assert(sizeof(double) == sizeof(uint64_t));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float get_float_s(Cpu &cpu, uint8_t reg) {
    bool fr = cpu.cop0.reg.status.fr;
    return bits_to_float(cpu.cop1.get_fgr_word_arith(reg, fr));
}

void set_float_s(Cpu &cpu, uint8_t reg, float value) {
    bool fr = cpu.cop0.reg.status.fr;
    cpu.cop1.set_fgr_word_arith(reg, float_to_bits(value), fr);
}

double get_float_d(Cpu &cpu, uint8_t reg) {
    bool fr = cpu.cop0.reg.status.fr;
    return bits_to_double(cpu.cop1.get_fgr_dword(reg, fr));
}

void set_float_d(Cpu &cpu, uint8_t reg, double value) {
    bool fr = cpu.cop0.reg.status.fr;
    cpu.cop1.set_fgr_dword(reg, double_to_bits(value), fr);
}

int32_t float_to_w_rounded(float value, uint8_t rounding_mode) {
    switch (rounding_mode & 3) {
    case 1: // toward zero
        return static_cast<int32_t>(std::trunc(value));
    case 2: // toward +inf
        return static_cast<int32_t>(std::ceil(value));
    case 3: // toward -inf
        return static_cast<int32_t>(std::floor(value));
    case 0: // nearest (ties to even via nearbyint)
    default:
        return static_cast<int32_t>(std::nearbyint(value));
    }
}

bool evaluate_c_cond_s(float fs, float ft, uint8_t cond) {
    const bool unordered = std::isnan(fs) || std::isnan(ft);
    const bool less = !unordered && (fs < ft);
    const bool equal = !unordered && (fs == ft);
    bool result = false;
    if (cond & 0x4) {
        result = result || less;
    }
    if (cond & 0x2) {
        result = result || equal;
    }
    if (cond & 0x1) {
        result = result || unordered;
    }
    return result;
}

} // namespace

bool FpuImpl::test_cop1_usable_exception(Cpu &cpu) {
    // https://n64brew.dev/wiki/COP1
    if (cpu.cop0.reg.status.cu1 == 0) {
        cpu.handle_exception(ExceptionCode::COPROCESSOR_UNUSABLE, 1, true);
        return true;
    }
    return false;
}

void FpuImpl::op_cfc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    uint8_t fs = inst.r_type.rd;
    int32_t value;
    switch (fs) {
    case 0: {
        value = cpu.cop1.fcr0;
    } break;
    case 31: {
        value = cpu.cop1.fcr31.raw;
    } break;
    default: {
        Utils::abort("CFC1: Unknown FS: {}", fs);
    } break;
    }
    cpu.gpr.write(inst.r_type.rt, (int64_t)value);
    Utils::instruction_trace("CFC1 FCR[{}], {}", fs, GPR_NAMES[inst.r_type.rt]);
}

void FpuImpl::op_ctc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    uint8_t fs = inst.r_type.rd;
    uint32_t value = cpu.gpr.read(inst.r_type.rt);
    switch (fs) {
    case 0: {
        Utils::abort("fcr0 is read only");
    } break;
    case 31: {
        value &= 0x183ffff;
        cpu.cop1.fcr31.raw = value;
    } break;
    default: {
        Utils::abort("CTC1: Unknown FS: {}", fs);
    } break;
    }
    Utils::trace("CTC1 FCR[{}], {}", fs, GPR_NAMES[inst.r_type.rt]);
}

void FpuImpl::op_mfc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    uint8_t fs = inst.r_type.rd;
    uint8_t rt = inst.r_type.rt;
    bool fr = cpu.cop0.reg.status.fr;
    int32_t value = static_cast<int32_t>(cpu.cop1.get_fgr_word(fs, fr));
    cpu.gpr.write(rt, static_cast<int64_t>(value));
    Utils::instruction_trace("MFC1 {}, FGR[{}]", GPR_NAMES[rt], fs);
}

void FpuImpl::op_mtc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    uint8_t fs = inst.r_type.rd;
    uint8_t rt = inst.r_type.rt;
    bool fr = cpu.cop0.reg.status.fr;
    uint32_t value = static_cast<uint32_t>(cpu.gpr.read(rt));
    cpu.cop1.set_fgr_word(fs, value, fr);
    Utils::instruction_trace("MTC1 {}, FGR[{}]", GPR_NAMES[rt], fs);
}

void FpuImpl::op_dmfc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    uint8_t fs = inst.r_type.rd;
    uint8_t rt = inst.r_type.rt;
    bool fr = cpu.cop0.reg.status.fr;
    uint64_t value = cpu.cop1.get_fgr_dword(fs, fr);
    cpu.gpr.write(rt, value);
    Utils::instruction_trace("DMFC1 {}, FGR[{}]", GPR_NAMES[rt], fs);
}

void FpuImpl::op_dmtc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    uint8_t fs = inst.r_type.rd;
    uint8_t rt = inst.r_type.rt;
    bool fr = cpu.cop0.reg.status.fr;
    uint64_t value = cpu.gpr.read(rt);
    cpu.cop1.set_fgr_dword(fs, value, fr);
    Utils::instruction_trace("DMTC1 {}, FGR[{}]", GPR_NAMES[rt], fs);
}

void FpuImpl::op_lwc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    int16_t offset = static_cast<int16_t>(inst.fi_type.offset);
    uint8_t ft = inst.fi_type.ft;
    uint8_t base = inst.fi_type.base;
    Utils::instruction_trace("LWC1 FGR[{}] <= *({} + {:#x})", ft,
                             GPR_NAMES[base], offset);
    uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);
    if (paddr.has_value()) {
        uint32_t word = Memory::read_paddr32(paddr.value());
        cpu.cop1.set_fgr_word(ft, word, cpu.cop0.reg.status.fr);
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::LOAD), 0, true);
    }
}

void FpuImpl::op_ldc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    int16_t offset = static_cast<int16_t>(inst.fi_type.offset);
    uint8_t ft = inst.fi_type.ft;
    uint8_t base = inst.fi_type.base;
    Utils::instruction_trace("LDC1 FGR[{}] <= *({} + {:#x})", ft,
                             GPR_NAMES[base], offset);
    uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);
    if (paddr.has_value()) {
        uint64_t dword = Memory::read_paddr64(paddr.value());
        cpu.cop1.set_fgr_dword(ft, dword, cpu.cop0.reg.status.fr);
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::LOAD), 0, true);
    }
}

void FpuImpl::op_swc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    int16_t offset = static_cast<int16_t>(inst.fi_type.offset);
    uint8_t ft = inst.fi_type.ft;
    uint8_t base = inst.fi_type.base;
    Utils::instruction_trace("SWC1 *({} + {:#x}) <= FGR[{}]", GPR_NAMES[base],
                             offset, ft);
    uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr =
        Mmu::resolve_vaddr(vaddr, Mmu::BusAccess::STORE);
    if (paddr.has_value()) {
        uint32_t word = cpu.cop1.get_fgr_word(ft, cpu.cop0.reg.status.fr);
        Memory::write_paddr32(paddr.value(), word);
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::STORE), 0, true);
    }
}

void FpuImpl::op_sdc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    int16_t offset = static_cast<int16_t>(inst.fi_type.offset);
    uint8_t ft = inst.fi_type.ft;
    uint8_t base = inst.fi_type.base;
    Utils::instruction_trace("SDC1 *({} + {:#x}) <= FGR[{}]", GPR_NAMES[base],
                             offset, ft);
    uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr =
        Mmu::resolve_vaddr(vaddr, Mmu::BusAccess::STORE);
    if (paddr.has_value()) {
        uint64_t dword = cpu.cop1.get_fgr_dword(ft, cpu.cop0.reg.status.fr);
        Memory::write_paddr64(paddr.value(), dword);
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::STORE), 0, true);
    }
}

void FpuImpl::op_cop1_arith(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }

    const uint8_t fmt = inst.fr_type.fmt;
    const uint8_t funct = inst.fr_type.funct;
    const uint8_t ft = inst.fr_type.ft;
    const uint8_t fs = inst.fr_type.fs;
    const uint8_t fd = inst.fr_type.fd;
    const bool fr = cpu.cop0.reg.status.fr;

    if (fmt == COP1_FMT_W) {
        switch (funct) {
        case COP1_FUNCT_CVT_S: {
            int32_t word =
                static_cast<int32_t>(cpu.cop1.get_fgr_word_arith(fs, fr));
            set_float_s(cpu, fd, static_cast<float>(word));
            Utils::instruction_trace("CVT.S.W FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_CVT_D: {
            int32_t word =
                static_cast<int32_t>(cpu.cop1.get_fgr_word_arith(fs, fr));
            set_float_d(cpu, fd, static_cast<double>(word));
            Utils::instruction_trace("CVT.D.W FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        default:
            break;
        }
    }

    if (fmt == COP1_FMT_S) {
        if ((funct & 0b110000) == COP1_FUNCT_C_F) {
            const uint8_t cond = funct & 0b1111;
            float a = get_float_s(cpu, fs);
            float b = get_float_s(cpu, ft);
            cpu.cop1.fcr31.compare = evaluate_c_cond_s(a, b, cond) ? 1 : 0;
            Utils::instruction_trace("C.cond.S cond={:#x} FGR[{}], FGR[{}]",
                                     cond, fs, ft);
            return;
        }

        switch (funct) {
        case COP1_FUNCT_ADD: {
            set_float_s(cpu, fd, get_float_s(cpu, fs) + get_float_s(cpu, ft));
            Utils::instruction_trace("ADD.S FGR[{}], FGR[{}], FGR[{}]", fd, fs,
                                     ft);
            return;
        }
        case COP1_FUNCT_SUB: {
            set_float_s(cpu, fd, get_float_s(cpu, fs) - get_float_s(cpu, ft));
            Utils::instruction_trace("SUB.S FGR[{}], FGR[{}], FGR[{}]", fd, fs,
                                     ft);
            return;
        }
        case COP1_FUNCT_MUL: {
            set_float_s(cpu, fd, get_float_s(cpu, fs) * get_float_s(cpu, ft));
            Utils::instruction_trace("MUL.S FGR[{}], FGR[{}], FGR[{}]", fd, fs,
                                     ft);
            return;
        }
        case COP1_FUNCT_DIV: {
            set_float_s(cpu, fd, get_float_s(cpu, fs) / get_float_s(cpu, ft));
            Utils::instruction_trace("DIV.S FGR[{}], FGR[{}], FGR[{}]", fd, fs,
                                     ft);
            return;
        }
        case COP1_FUNCT_SQRT: {
            set_float_s(cpu, fd, std::sqrt(get_float_s(cpu, fs)));
            Utils::instruction_trace("SQRT.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_ABS: {
            set_float_s(cpu, fd, std::fabs(get_float_s(cpu, fs)));
            Utils::instruction_trace("ABS.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_MOV: {
            set_float_s(cpu, fd, get_float_s(cpu, fs));
            Utils::instruction_trace("MOV.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_NEG: {
            set_float_s(cpu, fd, -get_float_s(cpu, fs));
            Utils::instruction_trace("NEG.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_ROUND_W: {
            int32_t w =
                static_cast<int32_t>(std::nearbyint(get_float_s(cpu, fs)));
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("ROUND.W.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_TRUNC_W: {
            int32_t w =
                static_cast<int32_t>(std::trunc(get_float_s(cpu, fs)));
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("TRUNC.W.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_CEIL_W: {
            int32_t w = static_cast<int32_t>(std::ceil(get_float_s(cpu, fs)));
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("CEIL.W.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_FLOOR_W: {
            int32_t w = static_cast<int32_t>(std::floor(get_float_s(cpu, fs)));
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("FLOOR.W.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_CVT_D: {
            set_float_d(cpu, fd, static_cast<double>(get_float_s(cpu, fs)));
            Utils::instruction_trace("CVT.D.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_CVT_W: {
            int32_t w = float_to_w_rounded(get_float_s(cpu, fs),
                                          cpu.cop1.fcr31.rounding_mode);
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("CVT.W.S FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        default:
            break;
        }
    }

    if (fmt == COP1_FMT_D) {
        if ((funct & 0b110000) == COP1_FUNCT_C_F) {
            const uint8_t cond = funct & 0b1111;
            double a = get_float_d(cpu, fs);
            double b = get_float_d(cpu, ft);
            const bool unordered = std::isnan(a) || std::isnan(b);
            const bool less = !unordered && (a < b);
            const bool equal = !unordered && (a == b);
            bool result = false;
            if (cond & 0x4) {
                result = result || less;
            }
            if (cond & 0x2) {
                result = result || equal;
            }
            if (cond & 0x1) {
                result = result || unordered;
            }
            cpu.cop1.fcr31.compare = result ? 1 : 0;
            Utils::instruction_trace("C.cond.D cond={:#x} FGR[{}], FGR[{}]",
                                     cond, fs, ft);
            return;
        }

        switch (funct) {
        case COP1_FUNCT_ADD: {
            set_float_d(cpu, fd, get_float_d(cpu, fs) + get_float_d(cpu, ft));
            Utils::instruction_trace("ADD.D FGR[{}], FGR[{}], FGR[{}]", fd, fs,
                                     ft);
            return;
        }
        case COP1_FUNCT_SUB: {
            set_float_d(cpu, fd, get_float_d(cpu, fs) - get_float_d(cpu, ft));
            Utils::instruction_trace("SUB.D FGR[{}], FGR[{}], FGR[{}]", fd, fs,
                                     ft);
            return;
        }
        case COP1_FUNCT_MUL: {
            set_float_d(cpu, fd, get_float_d(cpu, fs) * get_float_d(cpu, ft));
            Utils::instruction_trace("MUL.D FGR[{}], FGR[{}], FGR[{}]", fd, fs,
                                     ft);
            return;
        }
        case COP1_FUNCT_DIV: {
            set_float_d(cpu, fd, get_float_d(cpu, fs) / get_float_d(cpu, ft));
            Utils::instruction_trace("DIV.D FGR[{}], FGR[{}], FGR[{}]", fd, fs,
                                     ft);
            return;
        }
        case COP1_FUNCT_SQRT: {
            set_float_d(cpu, fd, std::sqrt(get_float_d(cpu, fs)));
            Utils::instruction_trace("SQRT.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_ABS: {
            set_float_d(cpu, fd, std::fabs(get_float_d(cpu, fs)));
            Utils::instruction_trace("ABS.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_MOV: {
            set_float_d(cpu, fd, get_float_d(cpu, fs));
            Utils::instruction_trace("MOV.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_NEG: {
            set_float_d(cpu, fd, -get_float_d(cpu, fs));
            Utils::instruction_trace("NEG.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_ROUND_W: {
            int32_t w =
                static_cast<int32_t>(std::nearbyint(get_float_d(cpu, fs)));
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("ROUND.W.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_TRUNC_W: {
            int32_t w =
                static_cast<int32_t>(std::trunc(get_float_d(cpu, fs)));
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("TRUNC.W.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_CEIL_W: {
            int32_t w = static_cast<int32_t>(std::ceil(get_float_d(cpu, fs)));
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("CEIL.W.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_FLOOR_W: {
            int32_t w = static_cast<int32_t>(std::floor(get_float_d(cpu, fs)));
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("FLOOR.W.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_CVT_S: {
            set_float_s(cpu, fd, static_cast<float>(get_float_d(cpu, fs)));
            Utils::instruction_trace("CVT.S.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        case COP1_FUNCT_CVT_W: {
            double d = get_float_d(cpu, fs);
            int32_t w;
            switch (cpu.cop1.fcr31.rounding_mode & 3) {
            case 1:
                w = static_cast<int32_t>(std::trunc(d));
                break;
            case 2:
                w = static_cast<int32_t>(std::ceil(d));
                break;
            case 3:
                w = static_cast<int32_t>(std::floor(d));
                break;
            default:
                w = static_cast<int32_t>(std::nearbyint(d));
                break;
            }
            cpu.cop1.set_fgr_word_arith(fd, static_cast<uint32_t>(w), fr);
            Utils::instruction_trace("CVT.W.D FGR[{}], FGR[{}]", fd, fs);
            return;
        }
        default:
            break;
        }
    }

    Utils::abort(
        "Unimplemented COP1 arith fmt={:#07b} funct={:#08b}", fmt, funct);
}

void FpuImpl::op_bc1(Cpu &cpu, instruction_t inst) {
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    const uint8_t ndtf = inst.i_type.rt;
    const bool cond = cpu.cop1.fcr31.compare != 0;
    switch (ndtf) {
    case COP1_BC_F:
        Utils::instruction_trace("BC1F");
        Cpu::branch_offset16(cpu, !cond, inst);
        return;
    case COP1_BC_T:
        Utils::instruction_trace("BC1T");
        Cpu::branch_offset16(cpu, cond, inst);
        return;
    case COP1_BC_FL:
        Utils::instruction_trace("BC1FL");
        Cpu::branch_likely_offset16(cpu, !cond, inst);
        return;
    case COP1_BC_TL:
        Utils::instruction_trace("BC1TL");
        Cpu::branch_likely_offset16(cpu, cond, inst);
        return;
    default:
        Utils::abort("Unimplemented BC1 rt={:#07b}", ndtf);
    }
}

} // namespace N64::Cpu

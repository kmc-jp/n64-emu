#include "fpu_instruction_impl.h"
#include "cpu/cpu.h"
#include "memory/bus.h"
#include "mmu/mmu.h"
#include "mmu/tlb.h"
#include "utils/log.h"
#include <optional>

namespace N64::Cpu {

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

} // namespace N64::Cpu

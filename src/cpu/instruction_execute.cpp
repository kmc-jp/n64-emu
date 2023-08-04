#include "cpu_instruction_impl.h"
#include "memory/bus.h"
#include "memory/tlb.h"
#include "mmu/mmu.h"
#include "utils/utils.h"
#include <cstdint>
#include <optional>

namespace N64 {
namespace Cpu {

void Cpu::execute_instruction(instruction_t inst) {
    uint8_t op = inst.op;
    switch (op) {
    case OPCODE_SPECIAL: // various operations (R format)
    {
        switch (inst.r_type.funct) {
        case SPECIAL_FUNCT_ADD: // ADD
            return CpuImpl::op_add(*this, inst);
        case SPECIAL_FUNCT_ADDU: // ADDU
            return CpuImpl::op_addu(*this, inst);
        case SPECIAL_FUNCT_SUB: // SUB
            return CpuImpl::op_sub(*this, inst);
        case SPECIAL_FUNCT_SUBU: // SUBU
            return CpuImpl::op_subu(*this, inst);
        case SPECIAL_FUNCT_MULT: // MULT
            return CpuImpl::op_mult(*this, inst);
        case SPECIAL_FUNCT_MULTU: // MULTU
            return CpuImpl::op_multu(*this, inst);
        case SPECIAL_FUNCT_DIV: // DIV
            return CpuImpl::op_div(*this, inst);
        case SPECIAL_FUNCT_DIVU: // DIVU
            return CpuImpl::op_divu(*this, inst);
        case SPECIAL_FUNCT_SLL: // SLL
            return CpuImpl::op_sll(*this, inst);
        case SPECIAL_FUNCT_SRL: // SRL
            return CpuImpl::op_srl(*this, inst);
        case SPECIAL_FUNCT_SRA: // SRA
            return CpuImpl::op_sra(*this, inst);
        case SPECIAL_FUNCT_SRAV: // SRAV
            return CpuImpl::op_srav(*this, inst);
        case SPECIAL_FUNCT_SLLV: // SLLV
            return CpuImpl::op_sllv(*this, inst);
        case SPECIAL_FUNCT_SRLV: // SRLV
            return CpuImpl::op_srlv(*this, inst);
        case SPECIAL_FUNCT_SLT: // SLT
            return CpuImpl::op_slt(*this, inst);
        case SPECIAL_FUNCT_SLTU: // SLTU
            return CpuImpl::op_sltu(*this, inst);
        case SPECIAL_FUNCT_AND: // AND
            return CpuImpl::op_and(*this, inst);
        case SPECIAL_FUNCT_OR: // OR
            return CpuImpl::op_or(*this, inst);
        case SPECIAL_FUNCT_XOR: // XOR
            return CpuImpl::op_xor(*this, inst);
        case SPECIAL_FUNCT_JR: // JR
            return CpuImpl::op_jr(*this, inst);
        case SPECIAL_FUNCT_JALR: // JALR
            return CpuImpl::op_jalr(*this, inst);
        case SPECIAL_FUNCT_MFHI: // MFHI
            return CpuImpl::op_mfhi(*this, inst);
        case SPECIAL_FUNCT_MFLO: // MFLO
            return CpuImpl::op_mflo(*this, inst);
        default: {
            Utils::abort("Unimplemented funct = {:#08b} for opcode = SPECIAL.",
                         static_cast<uint32_t>(inst.r_type.funct));
        } break;
        }
    } break;
    case OPCODE_REGIMM: {
        switch (inst.i_type.rt) {
        case REGIMM_RT_BLTZ: // BLTZ
            return CpuImpl::op_bltz(*this, inst);
        case REGIMM_RT_BLTZL: // BLTZL
            return CpuImpl::op_bltzl(*this, inst);
        case REGIMM_RT_BGEZ: // BGEZ
            return CpuImpl::op_bgez(*this, inst);
        case REGIMM_RT_BGEZL: // BGEZL
            return CpuImpl::op_bgezl(*this, inst);
        case REGIMM_RT_BLTZAL: // BLTZAL
            return CpuImpl::op_bltzal(*this, inst);
        case REGIMM_RT_BGEZAL: // BGEZAL
            return CpuImpl::op_bgezal(*this, inst);
        default: {
            Utils::abort("Unimplemented rt = {:#07b} for opcode = REGIMM.",
                         static_cast<uint32_t>(inst.i_type.rt));
        } break;
        }
    } break;
    case OPCODE_J: // J (J format)
        return CpuImpl::op_j(*this, inst);
    case OPCODE_JAL: // JAL (J format)
        return CpuImpl::op_jal(*this, inst);
    case OPCODE_LUI: // LUI (I format)
        return CpuImpl::op_lui(*this, inst);
    case OPCODE_LW: // LW (I format)
        return CpuImpl::op_lw(*this, inst);
    case OPCODE_LWU: // LWU (I format)
        return CpuImpl::op_lwu(*this, inst);
    case OPCODE_LHU: // LHU (I format)
        return CpuImpl::op_lhu(*this, inst);
    case OPCODE_LD: // LD (I format)
        return CpuImpl::op_ld(*this, inst);
    case OPCODE_SW: // SW (I format)
        return CpuImpl::op_sw(*this, inst);
    case OPCODE_SD: // SD (I format)
        return CpuImpl::op_sd(*this, inst);
    case OPCODE_ADDI: // ADDI (I format)
        return CpuImpl::op_addi(*this, inst);
    case OPCODE_ADDIU: // ADDIU (I format)
        return CpuImpl::op_addiu(*this, inst);
    case OPCODE_DADDI: // DADDI (I format)
        return CpuImpl::op_daddi(*this, inst);
    case OPCODE_ANDI: // ANDI (I format)
        return CpuImpl::op_andi(*this, inst);
    case OPCODE_ORI: // ORI (I format)
        return CpuImpl::op_ori(*this, inst);
    case OPCODE_XORI: // XORI (I format)
        return CpuImpl::op_xori(*this, inst);
    case OPCODE_BEQ: // BEQ (I format)
        return CpuImpl::op_beq(*this, inst);
    case OPCODE_BEQL: // BEQL (I format)
        return CpuImpl::op_beql(*this, inst);
    case OPCODE_BNE: // BNE (I format)
        return CpuImpl::op_bne(*this, inst);
    case OPCODE_BNEL: // BNEL (I format)
        return CpuImpl::op_bnel(*this, inst);
    case OPCODE_BLEZ: // BLEZ (I format)
        return CpuImpl::op_blez(*this, inst);
    case OPCODE_BLEZL: // BLEZL (I format)
        return CpuImpl::op_blezl(*this, inst);
    case OPCODE_BGTZ: // BGTZ (I format)
        return CpuImpl::op_bgtz(*this, inst);
    case OPCODE_BGTZL: // BGTZL (I format)
        return CpuImpl::op_bgtzl(*this, inst);
    case OPCODE_CACHE: // CACHE
        return CpuImpl::op_cache();
    case OPCODE_SLTI: // SLTI
        return CpuImpl::op_slti(*this, inst);
    case OPCODE_SLTIU: // SLTIU
        return CpuImpl::op_sltiu(*this, inst);
    case OPCODE_CP0: // CP0 instructions
    {
        // https://hack64.net/docs/VR43XX.pdf p.86
        assert_encoding_is_valid(inst.copz_type1.should_be_zero == 0);
        switch (inst.copz_type1.sub) {
        case COP_MFC: // MFC0 (COPZ format)
            return CpuImpl::op_mfc0(*this, inst);
        case COP_MTC: // MTC0 (COPZ format)
            return CpuImpl::op_mtc0(*this, inst);
        case COP_DMFC: // DMFC0 (COPZ format)
            return CpuImpl::op_dmfc0(*this, inst);
        case COP_DMTC: // DMTC0 (COPZ format)
            return CpuImpl::op_dmtc0(*this, inst);
        default: {
            Utils::abort("Unimplemented CP0 inst. sub = {:07b}",
                         static_cast<uint8_t>(inst.copz_type1.sub));
        } break;
        }
    } break;
    case OPCODE_CP1: // CP1 instructions
    {
        switch (inst.r_type.rs) {
        case COP_CFC: // CFC1
            return CpuImpl::op_cfc1(*this, inst);
        default: {
            Utils::abort("Unimplemented rs = {:#07b} for opcode = CP1.",
                         static_cast<uint32_t>(inst.r_type.rs));
        } break;
        }
    } break;
    default: {
        Utils::abort("Unimplemented opcode = {:#04x} ({:#08b})", op, op);
    } break;
    }
}

} // namespace Cpu
} // namespace N64
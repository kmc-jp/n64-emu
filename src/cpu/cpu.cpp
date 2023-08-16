#include "cpu.h"
#include "cop0.h"
#include "cpu_instruction_impl.h"
#include "fpu_instruction_impl.h"
#include "instruction.h"
#include "memory/bus.h"
#include "memory/tlb.h"
#include "mmu/mmu.h"
#include "n64_system/interrupt.h"
#include "utils/utils.h"
#include <cstdint>

namespace N64 {
namespace Cpu {

Cpu Cpu::instance{};

void Cpu::reset() {
    Utils::debug("Resetting CPU");
    delay_slot = false;
    prev_delay_slot = false;
    cop0.reset();
    cop1.reset();

    prev_pc = 0;
    pc = 0;
    next_pc = 4;
}

void Cpu::dump() {
    Utils::info("======= Core dump =======");
    Utils::info("PC\t= {:#x}", pc);
    Utils::info("prevPC\t= {:#018x}\tnextPC\t= {:#018x}", prev_pc, next_pc);
    Utils::info("hi\t= {:#018x}\tlo\t= {:#018x}", hi, lo);
    for (int i = 0; i < 16; i++) {
        Utils::info("{}\t= {:#018x}\t{}\t= {:#018x}", GPR_NAMES[i], gpr.read(i),
                    GPR_NAMES[i + 16], gpr.read(i + 16));
    }
    Utils::info("");
    cop0.dump();
    cop1.dump();
    Utils::info("=========================");
}

void Cpu::set_pc64(uint64_t value) {
    prev_pc = pc;
    pc = value;
    next_pc = value + 4;
}

void Cpu::set_pc32(uint32_t value) {
    prev_pc = pc;
    pc = (int64_t)((int32_t)value);
    next_pc = value + 4;
}

uint64_t Cpu::get_pc64() const { return pc; }

bool Cpu::should_service_interrupt() const {
    bool interrupts_pending = (cop0.reg.status.im & cop0.reg.cause.interrupt_pending) != 0;
    bool interrupts_enabled = cop0.reg.status.ie == 1;
    bool currently_handling_exception = cop0.reg.status.exl == 1;
    bool currently_handling_error = cop0.reg.status.erl == 1;

    return interrupts_pending && interrupts_enabled &&
           !currently_handling_exception && !currently_handling_error;
}

void Cpu::step() {
    Utils::trace("");
    Utils::trace("CPU cycle starts PC={:#018x}", pc);

    // Compare interrupt
    if (cop0.reg.count == (cop0.reg.compare << 1)) {
        cop0.reg.cause.ip7 = true;
        N64System::check_interrupt();
    }

    // updates delay slot
    prev_delay_slot = delay_slot;
    delay_slot = false;

    // check for interrupt/exception
    if (should_service_interrupt()) {
        handle_exception(ExceptionCode::INTERRUPT, 0, false);
    }

    // instruction fetch
    std::optional<uint32_t> paddr_of_pc = Mmu::resolve_vaddr(pc);
    if (!paddr_of_pc.has_value()) {
        // FIXME: what if IF causes TLB miss?
        Utils::critical("PC fetch causes TLB miss");
        Utils::abort("Aborted");
    }

    instruction_t inst;
    inst.raw = {Memory::read_paddr32(paddr_of_pc.value())};
    Utils::trace("fetched inst = {:#010x} from pc = {:#018x}", inst.raw, pc);

    prev_pc = pc;
    pc = next_pc;
    next_pc += 4;

    execute_instruction(inst);

    cop0.reg.count += CPU_CYCLES_PER_INST;
    cop0.reg.count &= 0x1FFFFFFFF;
}

// https://github.com/SimoneN64/Kaizen/blob/74dccb6ac6a679acbf41b497151e08af6302b0e9/src/backend/core/registers/Cop0.cpp#L253
void Cpu::handle_exception(ExceptionCode exception_code,
                           uint8_t coprocessor_error, bool use_prev_pc) {
    bool old_exl = cop0.reg.status.exl;
    // FIXME: is this identical to pc??
    int64_t epc = use_prev_pc ? prev_pc : pc;

    if (cop0.reg.status.exl == 0) {
        if (prev_delay_slot) {
            cop0.reg.cause.branch_delay = 1;
            // FIXME: Is just minus 4 fine?
            epc -= 4;
        } else {
            cop0.reg.cause.branch_delay = 0;
        }
        cop0.reg.status.exl = 1;
        cop0.reg.epc = epc;
    }

    cop0.reg.cause.coprocessor_error = coprocessor_error;
    cop0.reg.cause.exception_code = static_cast<uint8_t>(exception_code);

    if (cop0.reg.status.bev == 1) {
        Utils::unimplemented("BEV is set");
    }

    // FIXME: Add case of TLB Error
    switch (exception_code) {
    case ExceptionCode::INTERRUPT: {
        // The log below outputs too much log. Should commented out
        // Utils::trace("Handling exception/interruption");
        set_pc32(0x80000180);
    } break;
    default: {
        Utils::critical("Unimplemented. exception code = {}",
                        static_cast<uint8_t>(exception_code));
        Utils::abort("Aborted");
    } break;
    }
}

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
        case SPECIAL_FUNCT_DADD: // DADD
            return CpuImpl::op_dadd(*this, inst);
        case SPECIAL_FUNCT_DADDU: // DADDU
            return CpuImpl::op_daddu(*this, inst);
        case SPECIAL_FUNCT_SUB: // SUB
            return CpuImpl::op_sub(*this, inst);
        case SPECIAL_FUNCT_SUBU: // SUBU
            return CpuImpl::op_subu(*this, inst);
        case SPECIAL_FUNCT_DSUB: // DSUB
            return CpuImpl::op_dsub(*this, inst);
        case SPECIAL_FUNCT_DSUBU: // DSUBU
            return CpuImpl::op_dsubu(*this, inst);
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
        case SPECIAL_FUNCT_NOR: // NOR
            return CpuImpl::op_nor(*this, inst);
        case SPECIAL_FUNCT_JR: // JR
            return CpuImpl::op_jr(*this, inst);
        case SPECIAL_FUNCT_JALR: // JALR
            return CpuImpl::op_jalr(*this, inst);
        case SPECIAL_FUNCT_MFHI: // MFHI
            return CpuImpl::op_mfhi(*this, inst);
        case SPECIAL_FUNCT_MFLO: // MFLO
            return CpuImpl::op_mflo(*this, inst);
        case SPECIAL_FUNCT_TGE: // TGE
            return CpuImpl::op_tge(*this, inst);
        case SPECIAL_FUNCT_TGEU: // TGEU
            return CpuImpl::op_tgeu(*this, inst);
        case SPECIAL_FUNCT_TLT: // TLT
            return CpuImpl::op_tlt(*this, inst);
        case SPECIAL_FUNCT_TLTU: // TLTU
            return CpuImpl::op_tltu(*this, inst);
        case SPECIAL_FUNCT_TEQ: // TEQ
            return CpuImpl::op_teq(*this, inst);
        case SPECIAL_FUNCT_TNE: // TNE
            return CpuImpl::op_tne(*this, inst);
        case SPECIAL_FUNCT_DSLL: // DSLL
            return CpuImpl::op_dsll(*this, inst);
        case SPECIAL_FUNCT_DSRL: // DSRL
            return CpuImpl::op_dsrl(*this, inst);
        case SPECIAL_FUNCT_DSRA: // DSRA
            return CpuImpl::op_dsra(*this, inst);
        case SPECIAL_FUNCT_DSLL32: // DSLL32
            return CpuImpl::op_dsll32(*this, inst);
        case SPECIAL_FUNCT_DSRL32: // DSRL32
            return CpuImpl::op_dsrl32(*this, inst);
        case SPECIAL_FUNCT_DSRA32: // DSRA32
            return CpuImpl::op_dsra32(*this, inst);
        case SPECIAL_FUNCT_SYNC: // SYNC
            return CpuImpl::op_sync(*this, inst);
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
    case OPCODE_LB: // LB (I format)
        return CpuImpl::op_lb(*this, inst);
    case OPCODE_LBU: // LBU (I format)
        return CpuImpl::op_lbu(*this, inst);
    case OPCODE_LH: // LH (I format)
        return CpuImpl::op_lh(*this, inst);
    case OPCODE_LHU: // LHU (I format)
        return CpuImpl::op_lhu(*this, inst);
    case OPCODE_LW: // LW (I format)
        return CpuImpl::op_lw(*this, inst);
    case OPCODE_LWU: // LWU (I format)
        return CpuImpl::op_lwu(*this, inst);
    case OPCODE_LUI: // LUI (I format)
        return CpuImpl::op_lui(*this, inst);
    case OPCODE_LD: // LD (I format)
        return CpuImpl::op_ld(*this, inst);
    case OPCODE_LDL: // LDL (I format)
        return CpuImpl::op_ldl(*this, inst);
    case OPCODE_LDR: // LDR (I format)
        return CpuImpl::op_ldr(*this, inst);
    case OPCODE_LL: // LL
        return CpuImpl::op_ll(*this, inst);
    case OPCODE_LLD: // LLD
        return CpuImpl::op_lld(*this, inst);
    case OPCODE_SB: // SB (I format)
        return CpuImpl::op_sb(*this, inst);
    case OPCODE_SH: // SH (I format)
        return CpuImpl::op_sh(*this, inst);
    case OPCODE_SW: // SW (I format)
        return CpuImpl::op_sw(*this, inst);
    case OPCODE_SD: // SD (I format)
        return CpuImpl::op_sd(*this, inst);
    case OPCODE_SDL: // SDL (I format)
        return CpuImpl::op_sdl(*this, inst);
    case OPCODE_SDR: // SDR (I format)
        return CpuImpl::op_sdr(*this, inst);
    case OPCODE_SC: // SC
        return CpuImpl::op_sc(*this, inst);
    case OPCODE_SCD: // SCD
        return CpuImpl::op_scd(*this, inst);
    case OPCODE_ADDI: // ADDI (I format)
        return CpuImpl::op_addi(*this, inst);
    case OPCODE_ADDIU: // ADDIU (I format)
        return CpuImpl::op_addiu(*this, inst);
    case OPCODE_DADDI: // DADDI (I format)
        return CpuImpl::op_daddi(*this, inst);
    case OPCODE_DADDIU: // DADDIU (I format)
        return CpuImpl::op_daddiu(*this, inst);
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
            return FpuImpl::op_cfc1(*this, inst);
        case COP_CTC: // CTC1
            return FpuImpl::op_ctc1(*this, inst);
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
#include "cpu.h"
#include "cop0.h"
#include "instruction.h"
#include "memory/bus.h"
#include "mmu/mmu.h"
#include <cassert>

namespace N64 {
namespace Cpu {

void Cpu::step() {
    spdlog::debug("");
    spdlog::debug("CPU cycle starts");

    // Compare interrupt
    if (cop0.reg[Cop0Reg::COUNT] == cop0.reg[Cop0Reg::COMPARE]) {
        cop0.get_cause()->ip7 = true;
    }

    // updates delay slot
    prev_delay_slot = delay_slot;
    delay_slot = false;

    // check for interrupt/exception
    // TODO: implement MI_INTR_MASK_REG?
    if (cop0.get_interrupt_pending_masked()) {
        uint8_t exc_code = cop0.get_cause()->exception_code;
        switch (exc_code) {
        case 0: // interrupt
        {
            spdlog::critical(
                "Unimplemented. interruption IP = 0b{:07b} mask = 0b{:07b}",
                (uint32_t)cop0.get_cause()->interrupt_pending,
                (uint32_t)cop0.get_status()->im);
            Utils::core_dump();
            exit(-1);
        } break;
        default: {
            spdlog::critical("Unimplemented. exception code = {}", exc_code);
            Utils::core_dump();
            exit(-1);
        } break;
        }
    }

    // instruction fetch
    uint32_t paddr_of_pc = Mmu::resolve_vaddr(pc);
    // spdlog::debug("vaddr 0x{:x} -> paddr 0x{:x}", pc, paddr_of_pc);
    instruction_t inst;
    inst.raw = {Memory::read_paddr32(paddr_of_pc)};
    spdlog::debug("fetched inst = 0x{:x} from pc = 0x{:x}", inst.raw, pc);

    pc = next_pc;
    next_pc += 4;

    execute_instruction(inst);

    cop0.reg[Cop0Reg::COUNT] += CPU_CYCLES_PER_INST;
    cop0.reg[Cop0Reg::COUNT] &= 0x1FFFFFFFF;
}

void Cpu::execute_instruction(instruction_t inst) {
    uint8_t op = inst.op;
    switch (op) {
    case OPCODE_SPECIAL: // various operations (R format)
    {
        switch (inst.r_type.funct) {
        case SPECIAL_FUNCT_SLL: // SLL
        {
            assert(inst.r_type.rs == 0);
            uint64_t rt = gpr.read(inst.r_type.rt);
            uint8_t sa = inst.r_type.sa;
            spdlog::debug("SLL: GPR[{}] <= GPR[{}] << {}",
                          (uint32_t)inst.r_type.rd, (uint32_t)inst.r_type.rt,
                          (uint32_t)inst.r_type.sa);
            gpr.write(inst.r_type.rd, rt << sa);
        } break;
        case SPECIAL_FUNCT_SLTU: // SLTU
        {
            assert(inst.r_type.sa == 0);
            uint64_t rs = gpr.read(inst.r_type.rs); // unsigned
            uint64_t rt = gpr.read(inst.r_type.rt); // unsigned
            spdlog::debug("SLTU GPR[{}] GPR[{}] GPR[{}]",
                          (uint32_t)inst.r_type.rs, (uint32_t)inst.r_type.rt,
                          (uint32_t)inst.r_type.rd);
            if (rs < rt) {
                gpr.write(inst.r_type.rd, 1);
            } else {
                gpr.write(inst.r_type.rd, 0);
            }
        } break;
        default: {
            spdlog::critical("Unimplemented funct = 0b{:b} for opcode(0x{:x}).",
                             (uint32_t)inst.r_type.funct, op);
            Utils::core_dump();
            exit(-1);
        } break;
        }
    } break;
    case OPCODE_LUI: // LUI (I format)
    {
        assert(inst.i_type.rs == 0);
        int64_t simm = (int16_t)inst.i_type.imm; // sext
        simm <<= 16;
        spdlog::debug("LUI: GPR[{}] <= 0x{:x}", (uint32_t)inst.i_type.rt,
                      (uint64_t)simm);
        gpr.write(inst.i_type.rt, simm);
    } break;
    case OPCODE_LW: // LW (I format)
    {
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        spdlog::debug("LW: GPR[{}] <= *(GPR[{}] + 0x{:x})",
                      (uint32_t)inst.i_type.rt, (uint32_t)inst.i_type.rs,
                      offset);
        uint64_t vaddr = gpr.read(inst.i_type.rs) + offset;
        uint32_t paddr = Mmu::resolve_vaddr(vaddr);
        uint32_t word = Memory::read_paddr32(paddr);
        gpr.write(inst.i_type.rt, word);
    } break;
    case OPCODE_ADDIU: // ADDIU (I format)
    {
        int64_t imm = (int16_t)inst.i_type.imm; // sext
        int64_t tmp = gpr.read(inst.i_type.rs) + imm;
        spdlog::debug("ADDIU: GPR[{}] <= GPR[{}] + 0x{:x}",
                      (uint32_t)inst.i_type.rt, (uint32_t)inst.i_type.rs, imm);
        gpr.write(inst.i_type.rt, tmp);
    } break;
    case OPCODE_BNE: // BNE (I format)
    {
        // FIXME: 飛び先は今の命令+4+offsetであってる?
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        offset <<= 2;
        spdlog::debug("BNE: GPR[{}] != GPR[{}], pc+0x{:x}",
                      (uint32_t)inst.i_type.rs, (uint32_t)inst.i_type.rt,
                      (int64_t)offset);
        branch(gpr.read(inst.i_type.rs) != gpr.read(inst.i_type.rt),
               pc + offset);
    } break;
    case OPCODE_CACHE: // CACHE
    {
        // B.1.1 CACHE Instruction
        // https://hack64.net/docs/VR43XX.pdf
        // no need for emulation?
        spdlog::debug("CACHE: no effect");

    } break;
    case OPCODE_COP0: // CP0 instructions
    {
        assert(inst.copz_type1.should_be_zero == 0);
        switch (inst.copz_type1.sub) {
        case CP0_SUB_MT: // MTC0 (COPZ format)
        {
            spdlog::debug("MTC0: COP0.reg[{}] <= GPR[{}]",
                          (uint32_t)inst.copz_type1.rd,
                          (uint32_t)inst.copz_type1.rt);
            uint64_t tmp = gpr.read(inst.copz_type1.rt);
            cop0.reg[inst.copz_type1.rd] = tmp;
        } break;
        default: {
            spdlog::critical("Unimplemented CP0 inst. sub = 0b{:b}",
                             (uint32_t)inst.copz_type1.sub);
            Utils::core_dump();
            exit(-1);
        }
        }
    } break;
    default: {
        spdlog::critical("Unimplemented opcode = 0x{:02x} (0b{:06b})", op, op);
        Utils::core_dump();
        exit(-1);
    }
    }
}

void Cpu::branch(bool cond, uint64_t addr) {
    delay_slot = true;
    if (cond) {
        spdlog::debug("branch taken");
        next_pc = addr;
    } else {
        spdlog::debug("branch not taken");
    }
}

} // namespace Cpu

Cpu::Cpu n64cpu{};

} // namespace N64

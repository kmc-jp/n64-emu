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

    // TODO: delay slotの管理が必要
    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/Interpreter.hpp#L13

    // Compare interrupt
    if (cop0.reg[Cop0Reg::COUNT] == cop0.reg[Cop0Reg::COMPARE]) {
        cop0.get_cause()->ip7 = true;
    }

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
            dump();
            exit(-1);
        } break;
        default: {
            spdlog::critical("Unimplemented. exception code = {}", exc_code);
            dump();
            exit(-1);
        } break;
        }
    }

    // instruction fetch
    uint32_t paddr_of_pc = Mmu::resolve_vaddr(pc);
    // spdlog::debug("vaddr 0x{:x} -> paddr 0x{:x}", pc, paddr_of_pc);
    instruction_t inst;
    inst.raw = {Memory::read_paddr32(paddr_of_pc)};
    pc += 4;
    spdlog::debug("fetched inst = 0x{:x} from pc = 0x{:x}", inst.raw, pc);

    execute_instruction(inst);

    cop0.reg[Cop0Reg::COUNT] += CPU_CYCLES_PER_INST;
    cop0.reg[Cop0Reg::COUNT] &= 0x1FFFFFFFF;
}

void Cpu::execute_instruction(instruction_t inst) {
    uint8_t op = inst.op;
    switch (op) {
    case OPCODE_LUI: // LUI (I format)
    {
        assert(inst.i_type.rs == 0);
        int64_t shifted = inst.i_type.imm << 16; // sext
        spdlog::debug("LUI: GPR[{}] <= 0x{:x}", (uint32_t)inst.i_type.rt,
                      (uint64_t)shifted);
        gpr.write(inst.i_type.rt, shifted);
    } break;
    case OPCODE_LW: // LW (I format)
    {
        int64_t offset = inst.i_type.imm; // sext
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
        int64_t imm = inst.i_type.imm; // sext
        int64_t tmp = gpr.read(inst.i_type.rs) + imm;
        spdlog::debug("ADDIU: GPR[{}] <= GPR[{}] + 0x{:x}",
                      (uint32_t)inst.i_type.rt, (uint32_t)inst.i_type.rs, imm);
        gpr.write(inst.i_type.rt, tmp);
    } break;
    case OPCODE_BNE: // BNE (I format)
    {
        // FIXME: delay slotのエミュレーションが必要
        // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/interpreter/instructions.cpp#L155
        // FIXME: 飛び先は今の命令+4+offsetであってる?
        int64_t offset = inst.i_type.imm << 2; // sext
        spdlog::debug("BNE: GPR[{}], GPR[{}], pc+{}", (uint32_t)inst.i_type.rs,
                      (uint32_t)inst.i_type.rt, (int64_t)offset);
        if (gpr.read(inst.i_type.rs) != gpr.read(inst.i_type.rt)) {
            spdlog::debug("branch taken");
            pc += offset;
        } else {
            spdlog::debug("branch not taken");
        }
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
            dump();
            exit(-1);
        }
        }
    } break;
    default: {
        spdlog::critical("Unimplemented opcode = 0x{:02x} (0b{:06b})", op, op);
        dump();
        exit(-1);
    }
    }
}

} // namespace Cpu

Cpu::Cpu n64cpu{};

} // namespace N64

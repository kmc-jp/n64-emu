#include "cpu.h"
#include "../memory/bus.h"
#include "cop0.h"
#include "instruction.h"
#include <cassert>

namespace N64 {
namespace Cpu {

void Cpu::step() {
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
    Instruction inst = {Memory::read_paddr32(paddr_of_pc)};
    pc += 4;
    spdlog::debug("fetch inst = 0x{:x} from paddr = 0x{:x}", inst.raw,
                  paddr_of_pc);

    execute_instruction(inst);

    cop0.reg[Cop0Reg::COUNT] += CPU_CYCLES_PER_INST;
    cop0.reg[Cop0Reg::COUNT] &= 0x1FFFFFFFF;
}

void Cpu::execute_instruction(Instruction inst) {
    uint8_t op = inst.op;

    switch (op) {
    case OPCODE_LUI: // LUI
    {
        int32_t shifted = inst.imm << 16;
        int64_t sext = shifted; // sign extension
        spdlog::debug("LUI: GPR[{}] <= 0x{:x}", inst.rd, (uint64_t)sext);
        gpr.write(inst.rd, sext);
    } break;
    case OPCODE_COP0: // CP0 instructions
    {
        assert(inst.should_be_zero == 0);
        switch (inst.sub) {
        case CP0_SUB_MT: // MTC0
        {
            spdlog::debug("MTC0: COP0.reg[{}] <= GPR[{}]", inst.rd, inst.rt);
            uint64_t tmp = gpr.read(inst.rt);
            cop0.reg[inst.rd] = tmp;
        } break;
        default: {
            spdlog::critical("Unimplemented CP0 inst. sub = 0b{:b}", inst.sub);
            dump();
            exit(-1);
        }
        }
    } break;
    default: {
        spdlog::critical("Unimplemented opcode. opcode = 0x{:02x} (0b{:06b})",
                         op, op);
        dump();
        exit(-1);
    }
    }
}

} // namespace Cpu

Cpu::Cpu n64cpu{};

} // namespace N64

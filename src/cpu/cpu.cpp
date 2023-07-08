#include "cpu.h"
#include "../memory/bus.h"
#include "instruction.h"
#include <cassert>

namespace N64 {

void Cpu::step() {
    // instruction fetch
    uint32_t paddr_of_pc = Mmu::resolve_vaddr(pc);
    // spdlog::debug("vaddr 0x{:x} -> paddr 0x{:x}", pc, paddr_of_pc);

    Instruction inst = {Bus::read_paddr32(paddr_of_pc)};
    spdlog::debug("fetch inst = 0x{:x} from paddr = 0x{:x}", inst.raw,
                  paddr_of_pc);
    uint8_t op = inst.op;

    switch (op) {
    case OPCODE_LUI: {
        int32_t shifted = inst.imm << 16;
        int64_t sext = shifted; // sign extension
        spdlog::info("LUI: GPR[{}] <= 0x{:x}", inst.rd, (uint64_t)sext);
        n64cpu.gpr[inst.rd] = sext;
    } break;
    case OPCODE_COP0: {
        // CP0 instructions
        assert(inst.should_be_zero == 0);
        switch (inst.sub) {
        case CP0_SUB_MT: {
            // MTC0
            spdlog::info("MTC0: COP0.reg[{}] <= GPR[{}]", inst.rd, inst.rt);
            n64cpu.cop0.reg[inst.rd] = n64cpu.gpr[inst.rt];
        } break;
        default: {
            spdlog::critical("Unimpl CP0 inst. sub = 0b{:b}", inst.sub);
            N64::n64cpu.dump();
            exit(-1);
        }
        }
    } break;
    default: {
        spdlog::critical("Unimple inst. opcode = 0x{:02x} (0b{:06b})", op, op);
        N64::n64cpu.dump();
        exit(-1);
    }
    }

    pc += 4;
}

Cpu n64cpu{};

} // namespace N64

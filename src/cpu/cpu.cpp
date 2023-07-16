#include "cpu.h"
#include "cop0.h"
#include "cpu_operation.h"
#include "instruction.h"
#include "memory/bus.h"
#include "mmu/mmu.h"

namespace N64 {
namespace Cpu {

Cpu Cpu::instance{};

void Cpu::reset() {
    delay_slot = false;
    prev_delay_slot = false;
    cop0.reset();
}

void Cpu::dump() {
    spdlog::info("======= Core dump =======");
    spdlog::info("PC\t= {:#x}", pc);
    for (int i = 0; i < 16; i++) {
        spdlog::info("{}\t= {:#018x}\t{}\t= {:#018x}", GPR_NAMES[i],
                     gpr.read(i), GPR_NAMES[i + 16], gpr.read(i + 16));
    }
    spdlog::info("");
    cop0.dump();
    spdlog::info("=========================");
}

void Cpu::step() {
    spdlog::debug("");
    spdlog::debug("CPU cycle starts");

    // Compare interrupt
    if (cop0.reg.count == cop0.reg.compare) {
        cop0.reg.cause.ip7 = true;
    }

    // updates delay slot
    prev_delay_slot = delay_slot;
    delay_slot = false;

    // check for interrupt/exception
    // TODO: implement MI_INTR_MASK_REG?
    if (cop0.reg.cause.interrupt_pending & cop0.reg.status.im) {
        uint8_t exc_code = cop0.reg.cause.exception_code;
        switch (exc_code) {
        case 0: // interrupt
        {
            spdlog::critical(
                "Unimplemented. interruption IP = {:#010b} mask = {:#010b}",
                static_cast<uint32_t>(cop0.reg.cause.interrupt_pending),
                static_cast<uint32_t>(cop0.reg.status.im));
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
    instruction_t inst;
    inst.raw = {Memory::read_paddr32(paddr_of_pc)};
    spdlog::debug("fetched inst = {:#010x} from pc = {:#018x}", inst.raw, pc);

    pc = next_pc;
    next_pc += 4;

    execute_instruction(inst);

    cop0.reg.count += CPU_CYCLES_PER_INST;
    cop0.reg.count &= 0x1FFFFFFFF;
}

void Cpu::execute_instruction(instruction_t inst) {
    Operation::execute(*this, inst);
}

} // namespace Cpu

} // namespace N64
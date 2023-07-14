#include "cpu.h"
#include "cop0.h"
#include "cpu_operation.h"
#include "instruction.h"
#include "memory/bus.h"
#include "mmu/mmu.h"

namespace N64 {
namespace Cpu {

void Cpu::reset() { cop0.reset(); }

void Cpu::step() {
    spdlog::debug("");
    spdlog::debug("CPU cycle starts");

    // Compare interrupt
    if (cop0.reg[Cop0Reg::COUNT] == cop0.reg[Cop0Reg::COMPARE]) {
        cop0.get_cause_ref()->ip7 = true;
    }

    // updates delay slot
    prev_delay_slot = delay_slot;
    delay_slot = false;

    // check for interrupt/exception
    // TODO: implement MI_INTR_MASK_REG?
    if (cop0.get_interrupt_pending_masked()) {
        uint8_t exc_code = cop0.get_cause_ref()->exception_code;
        switch (exc_code) {
        case 0: // interrupt
        {
            spdlog::critical(
                "Unimplemented. interruption IP = {:#010b} mask = {:#010b}",
                static_cast<uint32_t>(cop0.get_cause_ref()->interrupt_pending),
                static_cast<uint32_t>(cop0.get_status_ref()->im));
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

    cop0.reg[Cop0Reg::COUNT] += CPU_CYCLES_PER_INST;
    cop0.reg[Cop0Reg::COUNT] &= 0x1FFFFFFFF;
}

void Cpu::execute_instruction(instruction_t inst) {
    Operation::execute(*this, inst);
}

} // namespace Cpu

Cpu::Cpu n64cpu{};

} // namespace N64
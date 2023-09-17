#include "rcp/rsp.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "utils/utils.h"

namespace N64 {
namespace Rsp {

// https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/RSP.cpp#L11
void Rsp::reset() {
    Utils::debug("Resetting RSP");
    set_pc(0);
    status_reg.raw = 0;
    status_reg.halt = 1;
    // TODO: add more registers
}

void Rsp::step() {
    // TODO: implement
    if (status_reg.halt)
        return;

    if (status_reg.single_step)
        Utils::unimplemented("RSP single step");

    Utils::unimplemented("RSP step");
}

void Rsp::set_pc(uint16_t value) {
    pc = value;
    next_pc = value + 4;
}

// https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/SPRegistersHandler.cpp#L67
uint32_t Rsp::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_SP_PC:
        return pc & 0xffc;
    default:
        Utils::abort("Unknown read from RSP paddr: {:#010x}", paddr);
    }
}

void Rsp::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_SP_PC: {
        pc = value & 0xffc;
    } break;
    case PADDR_SP_STATUS: {
        status_reg_write(value);
    } break;
    default:
        Utils::abort("Unknown write to RSP paddr: {:#010x}", paddr);
    }
}

void Rsp::status_reg_write(uint32_t value) {
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/SPRegistersHandler.cpp#L147
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/rsp_interface.c#L46
    sp_status_write_t write;
    write.raw = value;

    if (write.clear_halt && !write.set_halt)
        status_reg.halt = 0;
    if (!write.clear_halt && write.set_halt)
        status_reg.halt = 1;

    if (write.clear_broke)
        status_reg.broke = false;

    if (write.clear_intr) {
        g_mi().get_reg_intr().sp = 0;
        N64System::check_interrupt();
    }
    if (write.set_intr) {
        g_mi().get_reg_intr().sp = 1;
        N64System::check_interrupt();
    }

    status_reg.single_step =
        write.clear_sstep ? 0 : (write.set_sstep ? 1 : status_reg.single_step);
    status_reg.intr_on_break =
        write.clear_intr_on_break
            ? 0
            : (write.set_intr_on_break ? 1 : status_reg.intr_on_break);

    status_reg.signal_0 = write.clear_signal_0
                              ? 0
                              : (write.set_signal_0 ? 1 : status_reg.signal_0);
    status_reg.signal_1 = write.clear_signal_1
                              ? 0
                              : (write.set_signal_1 ? 1 : status_reg.signal_1);
    status_reg.signal_2 = write.clear_signal_2
                              ? 0
                              : (write.set_signal_2 ? 1 : status_reg.signal_2);
    status_reg.signal_3 = write.clear_signal_3
                              ? 0
                              : (write.set_signal_3 ? 1 : status_reg.signal_3);
    status_reg.signal_4 = write.clear_signal_4
                              ? 0
                              : (write.set_signal_4 ? 1 : status_reg.signal_4);
    status_reg.signal_5 = write.clear_signal_5
                              ? 0
                              : (write.set_signal_5 ? 1 : status_reg.signal_5);
    status_reg.signal_6 = write.clear_signal_6
                              ? 0
                              : (write.set_signal_6 ? 1 : status_reg.signal_6);
    status_reg.signal_7 = write.clear_signal_7
                              ? 0
                              : (write.set_signal_7 ? 1 : status_reg.signal_7);
}

Rsp Rsp::instance{};

} // namespace Rsp

Rsp::Rsp &g_rsp() { return Rsp::Rsp::get_instance(); }

} // namespace N64

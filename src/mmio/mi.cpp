#include "mi.h"
#include "n64_system/interrupt.h"
#include "utils.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace MI {

void MI::reset() {
    Utils::debug("Resetting MI");
    // TODO: what to do?
}

uint32_t MI::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_MI_MODE:
        return reg_mode;
    case PADDR_MI_VERSION:
        return 0x02020102;
    case PADDR_MI_INTERRUPT:
        return reg_intr.raw;
    case PADDR_MI_MASK: {
        return reg_intr_mask.raw;
    } break;
    default: {
        Utils::critical("Unimplemented. Read from MI paddr = {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void MI::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_MI_VERSION: {
        Utils::critical("Write to MI_VERSION, ignored.");
    } break;
    case PADDR_MI_MODE: {
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/MIPSInterfaceHandler.cpp#L79
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.c#L190
        reg_mode &= ~0x7f;
        reg_mode |= value & 0x7f;

        if (value & MiModeWriteFlag::CLR_INIT)
            reg_mode &= ~MiModeFlag::INIT;
        if (value & MiModeWriteFlag::SET_INIT)
            reg_mode |= MiModeFlag::INIT;
        if (value & MiModeWriteFlag::CLR_EBUS)
            reg_mode &= ~MiModeFlag::EBUS;
        if (value & MiModeWriteFlag::SET_EBUS)
            reg_mode |= MiModeFlag::EBUS;
        if (value & MiModeWriteFlag::CLR_DP_INTR) {
            reg_intr.dp = 0;
            N64System::check_interrupt();
        }
        if (value & MiModeWriteFlag::CLR_RDRAM)
            reg_mode &= ~MiModeFlag::RDRAM;
        if (value & MiModeWriteFlag::SET_RDRAM)
            reg_mode |= MiModeFlag::RDRAM;
    } break;
    case PADDR_MI_MASK: {
        if (value & 1)
            // clear sp interrupt mask
            reg_intr_mask.sp = 0;
        if (value & 2)
            // set sp interrupt mask
            reg_intr_mask.sp = 1;
        if (value & 4)
            // clear si interrupt mask
            reg_intr_mask.si = 0;
        if (value & 8)
            // set si interrupt mask
            reg_intr_mask.si = 1;
        if (value & 16)
            // clear ai interrupt mask
            reg_intr_mask.ai = 0;
        if (value & 32)
            // set ai interrupt mask
            reg_intr_mask.ai = 1;
        if (value & 64)
            // clear vi interrupt mask
            reg_intr_mask.vi = 0;
        if (value & 128)
            // set vi interrupt mask
            reg_intr_mask.vi = 1;
        if (value & 256)
            // clear pi interrupt mask
            reg_intr_mask.pi = 0;
        if (value & 512)
            // set pi interrupt mask
            reg_intr_mask.pi = 1;
        if (value & 1024)
            // clear dp interrupt mask
            reg_intr_mask.dp = 0;
        if (value & 2048)
            // set dp interrupt mask
            reg_intr_mask.dp = 1;
    } break;
    default: {
        Utils::critical(
            "Unimplemented. Write to MI paddr = {:#010x} Value = {:#010x}",
            paddr, value);
        Utils::abort("Aborted");
    } break;
    }
}

MI MI::instance{};

} // namespace MI
} // namespace Mmio
} // namespace N64
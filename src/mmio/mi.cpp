#include "mi.h"
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
    case PADDR_MODE:
        return reg_mode;
    case PADDR_VERSION:
        Utils::abort("Correct? Read from MI version");
        return reg_version;
    case PADDR_INTERRUPT:
        return reg_interrupt;
    case PADDR_MASK: {
        return reg_mask.raw;
    } break;
    default: {
        Utils::critical("Unimplemented. Read from MI paddr = {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void MI::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_VERSION: {
        // FIXME: correct? そもそもこのレジスタはreadされない?
        reg_version = value;
    } break;
    case PADDR_MASK: {
        if (value & 1)
            // clear sp interrupt mask
            reg_mask.sp = 0;
        if (value & 2)
            // set sp interrupt mask
            reg_mask.sp = 1;
        if (value & 4)
            // clear si interrupt mask
            reg_mask.si = 0;
        if (value & 8)
            // set si interrupt mask
            reg_mask.si = 1;
        if (value & 16)
            // clear ai interrupt mask
            reg_mask.ai = 0;
        if (value & 32)
            // set ai interrupt mask
            reg_mask.ai = 1;
        if (value & 64)
            // clear vi interrupt mask
            reg_mask.vi = 0;
        if (value & 128)
            // set vi interrupt mask
            reg_mask.vi = 1;
        if (value & 256)
            // clear pi interrupt mask
            reg_mask.pi = 0;
        if (value & 512)
            // set pi interrupt mask
            reg_mask.pi = 1;
        if (value & 1024)
            // clear dp interrupt mask
            reg_mask.dp = 0;
        if (value & 2048)
            // set dp interrupt mask
            reg_mask.dp = 1;
    } break;
    default: {
        Utils::critical("Unimplemented. Write to MI paddr = {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

MI MI::instance{};

} // namespace MI
} // namespace Mmio
} // namespace N64
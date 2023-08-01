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
        return reg_version;
    case PADDR_INTERRUPT:
        return reg_interrupt;
    case PADDR_MASK: {
        Utils::unimplemented("Read from MI MASK");
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
        Utils::unimplemented("Write to MI MASK");
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
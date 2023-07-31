#include "si.h"
#include "memory_map.h"
#include "utils.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace SI {

void SI::reset() {
    Utils::debug("Resetting SI");
    // TODO: what to do?
}

uint32_t SI::read_paddr32(uint32_t paddr) const {
    if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        uint64_t offset = paddr - PHYS_PIF_RAM_BASE;
        return Utils::read_from_byte_array32(pif_ram, offset);
    } else {
        Utils::critical("SI: Access to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    }
}

void SI::write_paddr32(uint32_t paddr, uint32_t value) {
    if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        Utils::write_to_byte_array32(&pif_ram[paddr - PHYS_PIF_RAM_BASE],
                                     value);
    } else {
        Utils::critical("SI: Access to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    }
}

SI SI::instance{};

} // namespace SI
} // namespace Mmio
} // namespace N64
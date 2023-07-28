#include "si.h"
#include "memory_map.h"
#include "utils.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace SI {

void SI::reset() {
    Utils::info("resetting SI");
    // TODO:
}

uint32_t SI::read_paddr32(uint32_t paddr) const {
    if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        return pif_ram[paddr - PHYS_PIF_RAM_BASE];
    } else {
        Utils::critical("SI: Access to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    }
}

void SI::write_paddr32(uint32_t paddr, uint32_t value) {
    if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        pif_ram[paddr - PHYS_PIF_RAM_BASE] = value;
    } else {
        Utils::critical("SI: Access to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    }
}

SI SI::instance{};

} // namespace SI
} // namespace Mmio
} // namespace N64
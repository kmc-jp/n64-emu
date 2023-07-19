#include "si.h"
#include "utils.h"

namespace N64 {
namespace Mmio {
namespace SI {

void SI::reset() {
    Utils::info("resetting SI");
    // TODO:
}

uint32_t SI::read_paddr32(uint32_t paddr) const {
    if (PADDR_PIF_RAM_BASE <= paddr && paddr <= PADDR_PIF_RAM_END) {
        return pif_ram[paddr - PADDR_PIF_RAM_BASE];
    } else {
        Utils::critical("SI: Access to paddr: {:#010x}", paddr);
        Utils::core_dump();
        exit(-1);
    }
}

void SI::write_paddr32(uint32_t paddr, uint32_t value) {
    if (PADDR_PIF_RAM_BASE <= paddr && paddr <= PADDR_PIF_RAM_END) {
        pif_ram[paddr - PADDR_PIF_RAM_BASE] = value;
    } else {
        Utils::critical("SI: Access to paddr: {:#010x}", paddr);
        Utils::core_dump();
        exit(-1);
    }
}

SI SI::instance{};

} // namespace SI
} // namespace Mmio
} // namespace N64
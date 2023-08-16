#include "mmio/si.h"
#include "memory/memory_map.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "utils/utils.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace SI {

void SI::reset() {
    Utils::debug("Resetting SI");
    // TODO: reset registers

    // non-registers
    dma_busy = 0;
}

uint32_t SI::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_SI_DRAM_ADDR: {
        return reg_dram_addr;
    } break;
    case PADDR_SI_STATUS: {
        uint32_t value = 0;
        value |= dma_busy ? 1 : 0;
        value |= 0 << 1;
        value |= 0 << 3;
        value |= g_mi().get_reg_intr().si << 12;
        return value;
    } break;
    default: {
        Utils::critical("SI: Read from paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void SI::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_SI_DRAM_ADDR: {
        reg_dram_addr = value & RDRAM_SIZE_MASK;
    } break;
    case PADDR_SI_STATUS: {
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/SerialInterfaceHandler.cpp#L98
        g_mi().get_reg_intr().si = 0;
        reg_status &= ~SiStatusFlags::INTERRUPT;
        N64System::check_interrupt();
    } break;
    default: {
        Utils::critical("SI: Write to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

SI SI::instance{};

} // namespace SI
} // namespace Mmio
} // namespace N64
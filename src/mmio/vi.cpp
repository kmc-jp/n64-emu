#include "vi.h"
#include "memory/memory_map.h"
#include "mi.h"
#include "n64_system/interrupt.h"
#include "utils.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace VI {

void VI::reset() {
    Utils::debug("Resetting VI");
    // TODO: reset registers
    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/VI.cpp#L12
    reg_status = 0;
}

uint32_t VI::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_VI_CTRL:
        return reg_status;
    default: {
        Utils::critical("VI: Read from paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void VI::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_VI_CTRL: {
        reg_status = value;
        // TODO: onChanged function?
    } break;
    default: {
        Utils::critical("VI: Write to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

VI VI::instance{};

} // namespace VI
} // namespace Mmio
} // namespace N64

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

    // TODO: correct?
    reg_origin = 0;
}

uint32_t VI::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_VI_CTRL:
        return reg_status;
    case PADDR_VI_ORIGIN:
        Utils::abort("VI: Read from VI_ORIGIN is not supported");
        return reg_origin;
    case PADDR_VI_WIDTH:
        Utils::abort("VI: Read from VI_WIDTH is not supported");
        return reg_width;
    case PADDR_VI_BURST:
        Utils::abort("VI: Read from VI_BURST is not supported");
        return reg_burst;
    case PADDR_VI_V_SYNC:
        Utils::abort("VI: Read from VI_V_SYNC is not supported");
        return reg_vsync;
    case PADDR_VI_H_SYNC:
        Utils::abort("VI: Read from VI_H_SYNC is not supported");
        return reg_hsync;
    case PADDR_VI_H_SYNC_LEAP:
        Utils::abort("VI: Read from VI_H_SYNC_LEAP is not supported");
        return reg_hsync_leap;
    case PADDR_VI_H_VIDEO:
        return reg_v_video;
    case PADDR_VI_V_VIDEO:
        return reg_h_video;
    case PADDR_VI_V_BURST:
        Utils::abort("VI: Read from VI_V_BURST is not supported");
        return reg_v_burst;
    case PADDR_VI_X_SCALE:
        Utils::abort("VI: Read from VI_X_SCALE is not supported");
        return reg_x_scale;
    case PADDR_VI_Y_SCALE:
        Utils::abort("VI: Read from VI_Y_SCALE is not supported");
        return reg_y_scale;
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
    case PADDR_VI_ORIGIN: {
        uint32_t masked = value & 0xFFFFFF;
        if (reg_origin != masked) {
            // swap?
            // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/VI.cpp#L55
        }
        reg_origin = masked;
        Utils::debug("VI: Origin set to {:#x}", reg_origin);
    } break;
    case PADDR_VI_WIDTH: {
        reg_width = value & 0x7ff;
        Utils::debug("VI: Width set to {:#x}", reg_width);
        // TODO: onChanged function?
    } break;
    case PADDR_VI_BURST: {
        // mask value of Kaizen may be wrong (not 10 bits but 12 bits)
        reg_burst = value;
        Utils::debug("VI: Burst set to {:#x}", reg_burst);
    } break;
    case PADDR_VI_V_SYNC: {
        // mask value of Kaizen may be wrong (not 10 bits but 12 bits)
        reg_vsync = value;
        Utils::debug("VI: V_Sync set to {:#x}", reg_vsync);
    } break;
    case PADDR_VI_H_SYNC: {
        // mask value of Kaizen may be wrong (not 10 bits but 12 bits)
        reg_hsync = value;
        Utils::debug("VI: H_Sync set to {:#x}", reg_hsync);
    } break;
    case PADDR_VI_H_SYNC_LEAP: {
        reg_hsync_leap = value;
        Utils::debug("VI: H_Sync_Leap set to {:#x}", reg_hsync_leap);
    } break;
    case PADDR_VI_H_VIDEO: {
        reg_h_video = value;
        Utils::debug("VI: H_Video set to {:#x}", reg_h_video);
    } break;
    case PADDR_VI_V_VIDEO: {
        reg_v_video = value;
        Utils::debug("VI: V_Video set to {:#x}", reg_v_video);
    } break;
    case PADDR_VI_V_BURST: {
        reg_v_burst = value;
        Utils::debug("VI: V_Burst set to {:#x}", reg_v_burst);
    } break;
    case PADDR_VI_X_SCALE: {
        reg_x_scale = value;
        Utils::debug("VI: X_Scale set to {:#x}", reg_x_scale);
    } break;
    case PADDR_VI_Y_SCALE: {
        reg_y_scale = value;
        Utils::debug("VI: Y_Scale set to {:#x}", reg_y_scale);
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

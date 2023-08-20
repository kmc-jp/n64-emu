#include "mmio/vi.h"
#include "memory/memory_map.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "utils/utils.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace VI {

void VI::reset() {
    Utils::debug("Resetting VI");
    // TODO: reset registers
    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/VI.cpp#L12
    reg_status = 0xf;
    reg_origin = 0;
    reg_width = 320;
    reg_intr = 0;
    reg_current = 0;
    reg_burst = 0; // FIXME: correct?
    reg_vsync = 0;
    reg_hsync = 0;
    reg_hsync_leap = 0; // FIXME: correct?

    // https://n64brew.dev/wiki/Video_Interface#0x0440_0018_-_VI_V_SYNC
    // Assume NTSC not PAL.
    num_half_lines = 0x20d / 2; // 262
    cycles_per_half_line = 1000;
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
    case PADDR_VI_INTR: // 0x0440000C
        Utils::abort("VI: Read from VI_WIDTH is not supported");
        return reg_intr;
    case PADDR_VI_V_CURRENT: // 0x04400010
    {
        // Project64: returns m_HalfLine
        // Kaizen: returns current << 1
        // n64: returns v_current
        // FIXME: correct?
        Utils::debug("VI: Burst Read value =  {:#x}", reg_current);
        return reg_current;
    } break;
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
    case PADDR_VI_INTR: // 0x0440000C
    {
        reg_intr = value & 0x3ff;
    } break;
    case PADDR_VI_V_CURRENT: // 0x04400010
    {
        // lower interrupt
        g_mi().get_reg_intr().vi = 0;
        N64System::check_interrupt();
    } break;
    case PADDR_VI_BURST: {
        reg_burst = value;
        Utils::debug("VI: Burst set to {:#x}", reg_burst);
    } break;
    case PADDR_VI_V_SYNC: {
        reg_vsync = value & 0x3FF;
        num_half_lines = reg_vsync / 2;
        Utils::debug("VI: V_Sync set to {:#x}", reg_vsync);
    } break;
    case PADDR_VI_H_SYNC: {
        // TODO: support PAL. ignore leap for now
        reg_hsync = value & 0x3FF;
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

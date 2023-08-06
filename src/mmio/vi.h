#ifndef VI_H
#define VI_H

#include "memory_map.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace VI {

constexpr uint32_t PADDR_VI_CTRL = 0x04400000;
constexpr uint32_t PADDR_VI_ORIGIN = 0x04400004;
constexpr uint32_t PADDR_VI_WIDTH = 0x04400008;
constexpr uint32_t PADDR_VI_INTR = 0x0440000C;
constexpr uint32_t PADDR_VI_V_CURRENT = 0x04400010;
constexpr uint32_t PADDR_VI_BURST = 0x04400014;
constexpr uint32_t PADDR_VI_V_SYNC = 0x04400018;
constexpr uint32_t PADDR_VI_H_SYNC = 0x0440001C;
constexpr uint32_t PADDR_VI_H_SYNC_LEAP = 0x04400020;
constexpr uint32_t PADDR_VI_H_VIDEO = 0x04400024;
constexpr uint32_t PADDR_VI_V_VIDEO = 0x04400028;
constexpr uint32_t PADDR_VI_V_BURST = 0x0440002C;
constexpr uint32_t PADDR_VI_X_SCALE = 0x04400030;
constexpr uint32_t PADDR_VI_Y_SCALE = 0x04400034;

// Video Interface
class VI {
  private:
    static VI instance;

    uint32_t reg_status;
    uint32_t reg_origin;
    uint32_t reg_width;
    // FIXME: this name is confusing. Correct?
    uint32_t reg_intr;
    uint32_t reg_current;
    uint32_t reg_burst;
    uint32_t reg_vsync;
    uint32_t reg_hsync;
    uint32_t reg_hsync_leap;
    uint32_t reg_h_video;
    uint32_t reg_v_video;
    uint32_t reg_v_burst;
    uint32_t reg_x_scale;
    uint32_t reg_y_scale;

  public:
    VI() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    inline static VI &get_instance() { return instance; }
};

} // namespace VI
} // namespace Mmio

inline Mmio::VI::VI &g_vi() { return Mmio::VI::VI::get_instance(); }

} // namespace N64

#endif
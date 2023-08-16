﻿#ifndef SI_H
#define SI_H

#include "memory/memory_map.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace SI {

constexpr uint32_t PADDR_SI_DRAM_ADDR = 0x04800000;
constexpr uint32_t PADDR_SI_PIF_AD_RD64B = 0x04800004;
constexpr uint32_t PADDR_SI_PIF_AD_WR64B = 0x04800010;

constexpr uint32_t PADDR_SI_STATUS = 0x04800018;

namespace SiStatusFlags {
enum SiStatusFlags : uint32_t {
    DMA_BUSY = 0x0001,
    RD_BUSY = 0x0002,
    DMA_ERROR = 0x0008,
    INTERRUPT = 0x1000,
};
}

// SI External Bus
class SI {
  public:
    SI() {}

    void reset();

    std::array<uint8_t, PIF_RAM_SIZE> &get_pif_ram() { return pif_ram; }

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    inline static SI &get_instance() { return instance; }

  private:
    std::array<uint8_t, PIF_RAM_SIZE> pif_ram;

    uint32_t reg_dram_addr;
    uint32_t reg_pif_addr;
    uint32_t reg_status;
    bool dma_busy;

    static SI instance;
};

} // namespace SI
} // namespace Mmio

inline Mmio::SI::SI &g_si() { return Mmio::SI::SI::get_instance(); }

} // namespace N64

#endif
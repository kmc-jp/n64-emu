#ifndef SI_H
#define SI_H

#include "utils.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace SI {

// https://n64brew.dev/wiki/Memory_map
constexpr uint32_t PADDR_PIF_RAM_BASE = 0x1FC007C0;
constexpr uint32_t PADDR_PIF_RAM_END = 0x1FC007FF;

constexpr uint32_t PIF_RAM_SIZE = PADDR_PIF_RAM_END - PADDR_PIF_RAM_BASE + 1;

// SI External Bus
class SI {
    uint8_t pif_ram[PIF_RAM_SIZE];

    static SI instance;

  public:
    SI() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    inline static SI &get_instance() { return instance; }
};

} // namespace SI
} // namespace Mmio

inline Mmio::SI::SI &g_si() { return Mmio::SI::SI::get_instance(); }

} // namespace N64

#endif
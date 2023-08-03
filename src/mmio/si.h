#ifndef SI_H
#define SI_H

#include "memory_map.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace SI {

constexpr uint32_t PADDR_SI_STATUS = 0x04800018;

// SI External Bus
class SI {
  private:
    std::array<uint8_t, PIF_RAM_SIZE> pif_ram;

    static SI instance;

  public:
    SI() {}

    void reset();

    std::array<uint8_t, PIF_RAM_SIZE> &get_pif_ram() { return pif_ram; }

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    inline static SI &get_instance() { return instance; }
};

} // namespace SI
} // namespace Mmio

inline Mmio::SI::SI &g_si() { return Mmio::SI::SI::get_instance(); }

} // namespace N64

#endif
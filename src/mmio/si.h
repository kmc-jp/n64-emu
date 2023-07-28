#ifndef SI_H
#define SI_H

#include "memory_map.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace SI {

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
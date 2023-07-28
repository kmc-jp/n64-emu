#ifndef MI_H
#define MI_H

#include "memory_map.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace MI {

const uint32_t PADDR_MODE = 0x04300000;
const uint32_t PADDR_VERSION = 0x04300004;
const uint32_t PADDR_INTERRUPT = 0x04300008;
const uint32_t PADDR_MASK = 0x0430000c;

// MIPS Interface
// https://n64brew.dev/wiki/MIPS_Interface
class MI {
  private:
    uint32_t reg_mode;
    uint32_t reg_version;
    uint32_t reg_interrupt;
    uint32_t reg_mask;

    static MI instance;

  public:
    MI() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    inline static MI &get_instance() { return instance; }
};

} // namespace MI
} // namespace Mmio

inline Mmio::MI::MI &g_mi() { return Mmio::MI::MI::get_instance(); }

} // namespace N64

#endif
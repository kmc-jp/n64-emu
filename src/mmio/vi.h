#ifndef VI_H
#define VI_H

#include "memory_map.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace VI {

constexpr uint32_t PADDR_VI_CTRL = 0x04400000;

// Video Interface
class VI {
  private:
    static VI instance;

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
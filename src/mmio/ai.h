#ifndef AI_H
#define AI_H

#include "memory_map.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace AI {

constexpr uint32_t PADDR_AI_STATUS = 0x0450000c;

// Audio Interface
class AI {
  private:
    static AI instance;

    uint32_t reg_status;

  public:
    AI() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    inline static AI &get_instance() { return instance; }
};

} // namespace AI
} // namespace Mmio

inline Mmio::AI::AI &g_ai() { return Mmio::AI::AI::get_instance(); }

} // namespace N64

#endif
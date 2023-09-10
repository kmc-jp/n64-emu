#ifndef AI_H
#define AI_H

#include "memory/memory_map.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace AI {

constexpr uint32_t PADDR_AI_DRAM_ADDR = 0x04500000;
constexpr uint32_t PADDR_AI_LENGTH = 0x04500004;
constexpr uint32_t PADDR_AI_CONTROL = 0x04500008;
constexpr uint32_t PADDR_AI_STATUS = 0x0450000c;
constexpr uint32_t PADDR_AI_DECRATE = 0x04500010;
constexpr uint32_t PADDR_AI_BITRATE = 0x04500014;

// Audio Interface
class AI {
  private:
    static AI instance;

    uint32_t reg_status;

    // AI DMAs have a double buffering mechanism.

    // 0 or 1
    int next_dma;
    uint32_t dma_addr[2];

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
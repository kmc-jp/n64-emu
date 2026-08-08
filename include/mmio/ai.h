#ifndef AI_H
#define AI_H

#include <cstdint>

namespace N64 {
namespace Mmio {
namespace AI {

// https://n64brew.dev/wiki/Audio_Interface
constexpr uint32_t PADDR_AI_DRAM_ADDR = 0x04500000;
constexpr uint32_t PADDR_AI_LENGTH = 0x04500004;
constexpr uint32_t PADDR_AI_CONTROL = 0x04500008;
constexpr uint32_t PADDR_AI_STATUS = 0x0450000c;
constexpr uint32_t PADDR_AI_DACRATE = 0x04500010;
constexpr uint32_t PADDR_AI_BITRATE = 0x04500014;

namespace AiStatusFlags {
enum AiStatusFlags : uint32_t {
    FULL = 1u << 31,
    BUSY = 1u << 30,
    ENABLED = 1u << 25,
    FULL_LO = 1u << 0,
};
}

namespace AIScheduler {
void on_dma_complete();
}

// Audio Interface
class AI {
    friend void AIScheduler::on_dma_complete();

  private:
    static AI instance;

    // Double-buffered DMA FIFO
    uint32_t dma_addr[2]{};
    uint32_t dma_length[2]{};
    int fifo_count{};

    // Next address programmed via AI_DRAM_ADDR (applied on LENGTH write)
    uint32_t next_dram_addr{};

    bool dma_enable{};
    uint32_t dacrate{};
    uint32_t bitrate{};

    // Delayed-carry bug: last DMA ended exactly on an 8 KiB page boundary
    bool delayed_carry{};

  public:
    AI() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    static AI &get_instance();

  private:
    void enqueue_dma(uint32_t length);
    void start_next_dma();
    uint64_t cycles_for_length(uint32_t length) const;
};

} // namespace AI
} // namespace Mmio

Mmio::AI::AI &g_ai();

} // namespace N64

#endif

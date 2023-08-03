#ifndef PI_H
#define PI_H

#include "utils.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace PI {

const uint32_t PADDR_DRAM_ADDR = 0x04600000;
const uint32_t PADDR_CART_ADDR = 0x04600004;
const uint32_t PADDR_RD_LEN = 0x04600008;
const uint32_t PADDR_WR_LEN = 0x0460000C;
const uint32_t PADDR_STATUS = 0x04600010;

// https://n64brew.dev/wiki/Peripheral_Interface#Domains
constexpr uint32_t POS_ROM_START = 0x1000'0000;
constexpr uint32_t POS_ROM_END = 0x1FFF'FFFF;

// https://n64brew.dev/wiki/Peripheral_Interface#0x0460_0010_-_PI_STATUS
namespace PiStatusFlags {
enum PiStatusFlags : uint32_t {
    DMA_BUSY = 1,
    IO_BUSY = 2,
    //ERROR = 4,
    INTERRUPT = 8,
};
}

namespace PiStatusWriteFlags {
enum PiStatusWriteFlags : uint32_t {
    RESET_DMA = 1,
    CLR_INTR = 2,
};
}

namespace PIScheduler {
void on_dma_write_completed();
}

// Peripheral Interface
// https://n64brew.dev/wiki/Peripheral_Interface
class PI {
    // DMA write完了時の処理
    friend void PIScheduler::on_dma_write_completed();

  private:
    uint32_t reg_dram_addr;
    uint32_t reg_cart_addr;
    uint32_t reg_rd_len;
    uint32_t reg_wr_len;
    uint32_t reg_status;

    static PI instance;

  public:
    PI() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    inline static PI &get_instance() { return instance; }

  private:
    void dma_write();

    void dma_read();
};

} // namespace PI
} // namespace Mmio

inline Mmio::PI::PI &g_pi() { return Mmio::PI::PI::get_instance(); }

} // namespace N64

#endif
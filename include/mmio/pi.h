#ifndef PI_H
#define PI_H

#include <cstdint>

namespace N64 {
namespace Mmio {
namespace PI {

const uint32_t PADDR_DRAM_ADDR = 0x04600000;
const uint32_t PADDR_CART_ADDR = 0x04600004;
const uint32_t PADDR_RD_LEN = 0x04600008;
const uint32_t PADDR_WR_LEN = 0x0460000C;
const uint32_t PADDR_STATUS = 0x04600010;
const uint32_t PADDR_BSD_DOM1_LAT = 0x04600014;
const uint32_t PADDR_BSD_DOM1_PWD = 0x04600018;
const uint32_t PADDR_BSD_DOM1_PGS = 0x0460001C;
const uint32_t PADDR_BSD_DOM1_RLS = 0x04600020;
const uint32_t PADDR_BSD_DOM2_LAT = 0x04600024;
const uint32_t PADDR_BSD_DOM2_PWD = 0x04600028;
const uint32_t PADDR_BSD_DOM2_PGS = 0x0460002C;
const uint32_t PADDR_BSD_DOM2_RLS = 0x04600030;

// https://n64brew.dev/wiki/Peripheral_Interface#Domains
constexpr uint32_t POS_ROM_START = 0x1000'0000;
constexpr uint32_t POS_ROM_END = 0x1FFF'FFFF;

// https://n64brew.dev/wiki/Peripheral_Interface#0x0460_0010_-_PI_STATUS
namespace PiStatusFlags {
enum PiStatusFlags : uint32_t {
    DMA_BUSY = 1,
    IO_BUSY = 2,
    // ERROR = 4,
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
void on_dma_read_completed();
}

// Peripheral Interface
// https://n64brew.dev/wiki/Peripheral_Interface
class PI {
    // Handlers invoked when DMA completes.
    friend void PIScheduler::on_dma_write_completed();
    friend void PIScheduler::on_dma_read_completed();

  private:
    uint32_t reg_dram_addr{};
    uint32_t reg_cart_addr{};
    uint32_t reg_rd_len{};
    uint32_t reg_wr_len{};
    uint32_t reg_status{};
    uint32_t reg_bsd_dom1_lat{};
    uint32_t reg_bsd_dom1_pwd{};
    uint32_t reg_bsd_dom1_pgs{};
    uint32_t reg_bsd_dom1_rls{};
    uint32_t reg_bsd_dom2_lat{};
    uint32_t reg_bsd_dom2_pwd{};
    uint32_t reg_bsd_dom2_pgs{};
    uint32_t reg_bsd_dom2_rls{};

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

Mmio::PI::PI &g_pi();

} // namespace N64

#endif

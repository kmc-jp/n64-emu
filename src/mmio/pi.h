#ifndef PI_H
#define PI_H

#include "utils.h"
#include <cstdint>
#include <spdlog/spdlog.h>

namespace N64 {
namespace Mmio {
namespace PI {

// https://n64brew.dev/wiki/Peripheral_Interface
const uint32_t PADDR_DRAM_ADDR = 0x04600000;
const uint32_t PADDR_CART_ADDR = 0x04600004;
const uint32_t PADDR_RD_LEN = 0x04600008;
const uint32_t PADDR_WR_LEN = 0x0460000C;
const uint32_t PADDR_STATUS = 0x04600010;

class PI {
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

    void on_dma_write_completed();
};

} // namespace PI
} // namespace Mmio

inline Mmio::PI::PI &g_pi() { return Mmio::PI::PI::get_instance(); }

} // namespace N64

#endif
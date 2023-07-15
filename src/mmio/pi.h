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
    uint32_t dram_addr;
    uint32_t cart_addr;
    uint32_t rd_len;
    uint32_t wr_len;
    uint32_t status;

    static PI instance;

  public:
    PI() {}

    void reset() {
      // TODO: what is the initial value?
    }

    uint32_t read_paddr32(uint32_t paddr) {
        switch (paddr) {
        case PADDR_DRAM_ADDR:
            return dram_addr;
        case PADDR_CART_ADDR:
            return cart_addr;
        case PADDR_RD_LEN:
            return rd_len;
        case PADDR_WR_LEN:
            return wr_len;
        case PADDR_STATUS:
            return status;
        default: {
            spdlog::critical("Unimplemented. Access to PI paddr = {:#010x}",
                             paddr);
            Utils::core_dump();
            exit(-1);
        } break;
        }
    }

    void write_paddr32(uint32_t paddr, uint32_t value) {
        switch (paddr) {
        case PADDR_DRAM_ADDR: {
            dram_addr = value;
        } break;
        case PADDR_CART_ADDR: {
            cart_addr = value;
        } break;
        case PADDR_RD_LEN: {
            rd_len = value;
        } break;
        case PADDR_WR_LEN: {
            wr_len = value;
        } break;
        case PADDR_STATUS: {
            status = value;
        } break;
        default: {
            spdlog::critical("Unimplemented. Access to PI paddr = {:#010x}",
                             paddr);
            Utils::core_dump();
            exit(-1);
        } break;
        }
    }

    static PI &get_instance() { return instance; }
};

} // namespace PI
} // namespace Mmio

inline Mmio::PI::PI &g_pi() { return Mmio::PI::PI::get_instance(); }

} // namespace N64

#endif
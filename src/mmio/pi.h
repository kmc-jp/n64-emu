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

    void reset() {
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PeripheralInterfaceHandler.cpp#L177
        reg_rd_len = 0x7f;
        reg_wr_len = 0x7f;
    }

    uint32_t read_paddr32(uint32_t paddr) {
        switch (paddr) {
        case PADDR_DRAM_ADDR:
            return reg_dram_addr;
        case PADDR_CART_ADDR:
            return reg_cart_addr;
        case PADDR_RD_LEN:
            return reg_rd_len;
        case PADDR_WR_LEN:
            return reg_wr_len;
        case PADDR_STATUS:
            return reg_status;
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
            reg_dram_addr = value;
        } break;
        case PADDR_CART_ADDR: {
            reg_cart_addr = value;
        } break;
        case PADDR_RD_LEN: {
            reg_rd_len = value;
            start_dma_transfer();
        } break;
        case PADDR_WR_LEN: {
            reg_wr_len = value;
            start_dma_transfer();
        } break;
        case PADDR_STATUS: {
            reg_status = value;
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

  private:
    void start_dma_transfer() {
        // TODO: statusレジスタのセット
        // TODO: DMAエンジン
        spdlog::critical("Unimplemented. DMA Transfer by RI");
        Utils::core_dump();
        exit(-1);
    }
};

} // namespace PI
} // namespace Mmio

inline Mmio::PI::PI &g_pi() { return Mmio::PI::PI::get_instance(); }

} // namespace N64

#endif
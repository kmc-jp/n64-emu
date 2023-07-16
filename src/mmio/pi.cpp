#include "pi.h"

namespace N64 {
namespace Mmio {
namespace PI {

void PI::reset() {
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PeripheralInterfaceHandler.cpp#L177
    reg_rd_len = 0x7f;
    reg_wr_len = 0x7f;
}

uint32_t PI::read_paddr32(uint32_t paddr) const {
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
        spdlog::critical("Unimplemented. Access to PI paddr = {:#010x}", paddr);
        Utils::core_dump();
        exit(-1);
    } break;
    }
}

void PI::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_DRAM_ADDR: {
        reg_dram_addr = value & 0x00FF'FFFF;
    } break;
    case PADDR_CART_ADDR: {
        reg_cart_addr = value;
    } break;
    case PADDR_RD_LEN: {
        reg_rd_len = (value & 0x00FF'FFFF);
        read_dma();
    } break;
    case PADDR_WR_LEN: {
        reg_wr_len = (value & 0x00FF'FFFF);
        write_dma();
    } break;
    case PADDR_STATUS: {
        reg_status = value;
        if (value & 0b001) {
            // Reset DMA controller and stop any transfer being done
            Utils::unimplemented("Reset DMA by RI");
        }
        if (value & 0b0010) {
            // Clear interrupt
            Utils::unimplemented("Clear interrupt by RI");
        }
    } break;
    default: {
        spdlog::critical("Unimplemented. Access to PI paddr = {:#010x}", paddr);
        Utils::core_dump();
        exit(-1);
    } break;
    }
}

void PI::write_dma() {
    // TODO: statusレジスタのセット
    // TODO: DMAエンジン
    Utils::unimplemented("DMA Transfer by RI");
}

void PI::read_dma() {
    // TODO: statusレジスタのセット
    // TODO: DMAエンジン
    Utils::unimplemented("DMA Transfer by RI");
}

PI PI::instance{};

} // namespace PI
} // namespace Mmio
} // namespace N64
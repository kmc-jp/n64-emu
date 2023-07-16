#include "pi.h"
#include "memory.h"

namespace N64 {
namespace Mmio {
namespace PI {

// https://n64brew.dev/wiki/Peripheral_Interface#Domains
constexpr uint32_t POS_ROM_START = 0x1000'0000;
constexpr uint32_t POS_ROM_END = 0x1FFF'FFFF;

// https://n64brew.dev/wiki/Peripheral_Interface#0x0460_0010_-_PI_STATUS
constexpr uint32_t PI_STATUS_DMA_BUSY = 0b0001;
constexpr uint32_t PI_STATUS_IO_BUSY = 0b0010;

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
        dma_read();
    } break;
    case PADDR_WR_LEN: {
        reg_wr_len = (value & 0x00FF'FFFF);
        dma_write();
    } break;
    case PADDR_STATUS: {
        if (value & 0b001) {
            // Reset DMA controller and stop any transfer being done
            Utils::unimplemented("Reset DMA by RI");
        }
        if (value & 0b0010) {
            // Clear interrupt
            Utils::unimplemented("Clear interrupt by RI");
        }
        // do not write to register
    } break;
    default: {
        spdlog::critical("Unimplemented. Access to PI paddr = {:#010x}", paddr);
        Utils::core_dump();
        exit(-1);
    } break;
    }
}

// DMA write完了時の処理
void PI::on_dma_write_completed() {
    reg_status &= ~PI_STATUS_DMA_BUSY;
    reg_status &= ~PI_STATUS_IO_BUSY;
}

void PI::dma_write() {
    // TODO: statusレジスタのセット
    uint32_t write_pos = reg_dram_addr & 0x7FFFFE;
    uint32_t read_pos = reg_cart_addr;
    // FIXME: カートリッジのサイズを超えてsegvになる可能性あり
    const uint32_t transfer_len = (reg_wr_len & 0x00FF'FFFF) + 1;
    for (uint32_t i = 0; i < transfer_len; i++) {
        g_memory().rdram[write_pos + i] =
            g_memory().rom.read_offset8(read_pos + i);
    }
    reg_status |= PI_STATUS_DMA_BUSY;
    reg_status |= PI_STATUS_IO_BUSY;

    // TODO: スケジューラで処理する. issue #30
    on_dma_write_completed();
}

void PI::dma_read() {
    // TODO: statusレジスタのセット
    // TODO: DMAエンジン
    Utils::unimplemented("DMA Transfer by RI");
}

PI PI::instance{};

} // namespace PI
} // namespace Mmio
} // namespace N64
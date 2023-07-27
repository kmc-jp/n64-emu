#include "pi.h"
#include "memory.h"
#include "n64_system/scheduler.h"
#include "utils.h"

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
    Utils::info("resetting PI");
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
        Utils::critical("Unimplemented. Access to PI paddr = {:#010x}", paddr);
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
        Utils::critical("Unimplemented. Access to PI paddr = {:#010x}", paddr);
        Utils::core_dump();
        exit(-1);
    } break;
    }
}

void PI::dma_write() {
    uint32_t dram_addr = reg_dram_addr & 0x7FFFFE;
    uint32_t cart_addr = reg_cart_addr;
    const uint32_t transfer_len = (reg_wr_len & 0x00FF'FFFF) + 1;

    if (0x1000'0000 <= cart_addr && cart_addr <= 0xFFFF'FFFF) {
        cart_addr -= 0x1000'0000;
        for (uint32_t i = 0; i < transfer_len; i++) {
            // FIXME: ROMのサイズを超えてsegvになる可能性あり
            g_memory().get_rdram()[dram_addr + i] =
                g_memory().rom.read_offset8(cart_addr + i);
        }

        reg_status |= PI_STATUS_DMA_BUSY;
        reg_status |= PI_STATUS_IO_BUSY;

        Utils::debug(
            "DMA Write: Cart {:#010x} -> dram {:#010x} (len = {:#010x})",
            cart_addr, dram_addr, transfer_len);
        // Timerの値は以下を参考
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PeripheralInterfaceHandler.cpp#L460
        g_scheduler().set_timer(
            transfer_len / 8,
            N64System::Event{&PIScheduler::on_dma_write_completed});
    } else {
        Utils::critical(
            "DMA Write cart addr = {:#010x} -> dram addr = {:#010x}", cart_addr,
            dram_addr);
        Utils::unimplemented("DMA Transfer by RI");
    }
}

void PIScheduler::on_dma_write_completed() {
    // TODO: cart_addrの範囲によってセットするレジスタが異なる
    g_pi().reg_status &= ~PI_STATUS_DMA_BUSY;
    g_pi().reg_status &= ~PI_STATUS_IO_BUSY;
    Utils::debug("DMA Write completed");
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
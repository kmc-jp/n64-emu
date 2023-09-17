#include "mmio/pi.h"
#include "memory/memory.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "n64_system/scheduler.h"
#include "utils/log.h"

namespace N64 {
namespace Mmio {
namespace PI {

void PI::reset() {
    Utils::debug("Resetting PI");
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PeripheralInterfaceHandler.cpp#L177
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
        Utils::critical("Unimplemented. Read from PI paddr = {:#010x}", paddr);
        Utils::abort("Aborted");
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
        if (value & PiStatusWriteFlags::RESET_DMA) {
            // Reset DMA controller and stop any transfer being done
            Utils::unimplemented("Reset DMA by RI");
        }
        if (value & PiStatusWriteFlags::CLR_INTR) {
            g_mi().get_reg_intr().pi = 0;
            reg_status &= ~PiStatusFlags::INTERRUPT;
            N64System::check_interrupt();
        }
    } break;
    default: {
        Utils::critical("Unimplemented. Write to PI paddr = {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void PI::dma_write() {
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PeripheralInterfaceHandler.cpp#L364
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

        reg_status |= PiStatusFlags::DMA_BUSY;

        Utils::debug("DMA Write: cart offset {:#010x} -> dram offset {:#010x} "
                     "(len = {:#010x})",
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
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/SystemTiming.cpp#L210
    g_pi().reg_status &= ~PiStatusFlags::DMA_BUSY;
    g_pi().reg_status |= PiStatusFlags::INTERRUPT;
    g_mi().get_reg_intr().pi = 1;
    N64System::check_interrupt();
    Utils::debug("DMA Write completed");
}

void PI::dma_read() {
    // TODO: implement
    Utils::unimplemented("DMA Transfer by RI");
}

PI PI::instance{};

} // namespace PI
} // namespace Mmio

Mmio::PI::PI &g_pi() { return Mmio::PI::PI::get_instance(); }

} // namespace N64

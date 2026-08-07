#include "mmio/pi.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "n64_system/scheduler.h"
#include "utils/byte_array.h"
#include "utils/log.h"

namespace N64 {
namespace Mmio {
namespace PI {

void PI::reset() {
    Utils::debug("Resetting PI");
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PeripheralInterfaceHandler.cpp#L177
    reg_dram_addr = 0;
    reg_cart_addr = 0;
    reg_rd_len = 0x7f;
    reg_wr_len = 0x7f;
    reg_status = 0;
    reg_bsd_dom1_lat = 0;
    reg_bsd_dom1_pwd = 0;
    reg_bsd_dom1_pgs = 0;
    reg_bsd_dom1_rls = 0;
    reg_bsd_dom2_lat = 0;
    reg_bsd_dom2_pwd = 0;
    reg_bsd_dom2_pgs = 0;
    reg_bsd_dom2_rls = 0;
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
    case PADDR_BSD_DOM1_LAT:
        return reg_bsd_dom1_lat;
    case PADDR_BSD_DOM1_PWD:
        return reg_bsd_dom1_pwd;
    case PADDR_BSD_DOM1_PGS:
        return reg_bsd_dom1_pgs;
    case PADDR_BSD_DOM1_RLS:
        return reg_bsd_dom1_rls;
    case PADDR_BSD_DOM2_LAT:
        return reg_bsd_dom2_lat;
    case PADDR_BSD_DOM2_PWD:
        return reg_bsd_dom2_pwd;
    case PADDR_BSD_DOM2_PGS:
        return reg_bsd_dom2_pgs;
    case PADDR_BSD_DOM2_RLS:
        return reg_bsd_dom2_rls;
    default: {
        Utils::critical("Unimplemented. Read from PI paddr = {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void PI::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_DRAM_ADDR: {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/interface/pi.c#L117
        reg_dram_addr = value;
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
            reg_status = 0;
        }
        if (value & PiStatusWriteFlags::CLR_INTR) {
            g_mi().get_reg_intr().pi = 0;
            reg_status &= ~PiStatusFlags::INTERRUPT;
            N64System::check_interrupt();
        }
    } break;
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/interface/pi.c#L214
    case PADDR_BSD_DOM1_LAT:
        reg_bsd_dom1_lat = value & 0xFF;
        break;
    case PADDR_BSD_DOM1_PWD:
        reg_bsd_dom1_pwd = value & 0xFF;
        break;
    case PADDR_BSD_DOM1_PGS:
        reg_bsd_dom1_pgs = value & 0xFF;
        break;
    case PADDR_BSD_DOM1_RLS:
        reg_bsd_dom1_rls = value & 0xFF;
        break;
    case PADDR_BSD_DOM2_LAT:
        reg_bsd_dom2_lat = value & 0xFF;
        break;
    case PADDR_BSD_DOM2_PWD:
        reg_bsd_dom2_pwd = value & 0xFF;
        break;
    case PADDR_BSD_DOM2_PGS:
        reg_bsd_dom2_pgs = value & 0xFF;
        break;
    case PADDR_BSD_DOM2_RLS:
        reg_bsd_dom2_rls = value & 0xFF;
        break;
    default: {
        Utils::critical("Unimplemented. Write to PI paddr = {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void PI::dma_write() {
    // https://n64brew.dev/wiki/Peripheral_Interface
    uint32_t length = (reg_wr_len & 0x00FF'FFFF) + 1;
    uint32_t cart_addr = reg_cart_addr & 0xFFFFFFFE;
    uint32_t dram_addr = reg_dram_addr & 0x007FFFFE;

    if ((dram_addr & 0x7) && length >= 0x7) {
        length -= dram_addr & 0x7;
    }
    reg_wr_len = length;

    auto &rdram = g_memory().get_rdram();
    auto &sram = g_memory().get_sram();

    if (!sram.empty() && PHYS_SRAM_BASE <= cart_addr &&
        cart_addr < PHYS_ROM_BASE) {
        const uint32_t sram_mask = static_cast<uint32_t>(sram.size() - 1);
        for (uint32_t i = 0; i < length; i++) {
            const uint32_t sram_offs =
                ((cart_addr - PHYS_SRAM_BASE) + i) & sram_mask;
            // SRAM is logical N64 byte order; RDRAM is host-endian (addr^3).
            Utils::write_to_byte_array8(rdram, (dram_addr + i) & RDRAM_SIZE_MASK,
                                       sram[sram_offs]);
        }
        Utils::debug("DMA Write: SRAM {:#010x} -> dram {:#010x} (len = {:#010x})",
                     cart_addr, dram_addr, length);
    } else if (0x1000'0000 <= cart_addr && cart_addr <= 0xFFFF'FFFF) {
        const uint32_t cart_offset = cart_addr - 0x1000'0000;
        for (uint32_t i = 0; i < length; i++) {
            Utils::write_to_byte_array8(
                rdram, (dram_addr + i) & RDRAM_SIZE_MASK,
                g_memory().rom.read_offset8(cart_offset + i));
        }

        Utils::debug("DMA Write: cart offset {:#010x} -> dram offset {:#010x} "
                     "(len = {:#010x})",
                     cart_offset, dram_addr, length);
    } else {
        Utils::critical(
            "DMA Write cart addr = {:#010x} -> dram addr = {:#010x}", cart_addr,
            dram_addr);
        Utils::unimplemented("DMA Transfer by PI");
        return;
    }

    reg_status |= PiStatusFlags::DMA_BUSY;
    reg_dram_addr = dram_addr + length;
    reg_cart_addr = cart_addr + length;
    g_scheduler().set_timer(
        length / 8,
        N64System::Event{&PIScheduler::on_dma_write_completed});
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
    // https://n64brew.dev/wiki/Peripheral_Interface
    uint32_t length = (reg_rd_len & 0x00FF'FFFF) + 1;
    uint32_t cart_addr = reg_cart_addr & 0xFFFFFFFE;
    uint32_t dram_addr = reg_dram_addr & 0x007FFFFE;

    if ((dram_addr & 0x7) && length >= 0x7) {
        length -= dram_addr & 0x7;
    }
    reg_rd_len = length;

    auto &rdram = g_memory().get_rdram();
    auto &sram = g_memory().get_sram();

    // RDRAM -> cartridge (typically SRAM)
    if (PHYS_SRAM_BASE <= cart_addr && cart_addr < PHYS_ROM_BASE) {
        if (!sram.empty()) {
            const uint32_t sram_mask = static_cast<uint32_t>(sram.size() - 1);
            for (uint32_t i = 0; i < length; i++) {
                const uint32_t sram_offs =
                    ((cart_addr - PHYS_SRAM_BASE) + i) & sram_mask;
                sram[sram_offs] = Utils::read_from_byte_array8(
                    rdram, (dram_addr + i) & RDRAM_SIZE_MASK);
            }
            Utils::debug(
                "DMA Read: dram {:#010x} -> SRAM {:#010x} (len = {:#010x})",
                dram_addr, cart_addr, length);
        } else {
            Utils::debug("DMA Read stub to cart {:#010x} len {:#010x} (no SRAM)",
                         cart_addr, length);
        }
    } else if (0x1000'0000 <= cart_addr) {
        Utils::warn("DMA Read to ROM ignored: cart {:#010x}", cart_addr);
    } else {
        Utils::critical("DMA Read cart addr = {:#010x}", cart_addr);
        Utils::unimplemented("DMA Transfer by PI");
        return;
    }

    reg_status |= PiStatusFlags::DMA_BUSY;
    reg_dram_addr = dram_addr + length;
    reg_cart_addr = cart_addr + length;
    g_scheduler().set_timer(length / 8,
                            N64System::Event{&PIScheduler::on_dma_read_completed});
}

void PIScheduler::on_dma_read_completed() {
    g_pi().reg_status &= ~PiStatusFlags::DMA_BUSY;
    g_pi().reg_status |= PiStatusFlags::INTERRUPT;
    g_mi().get_reg_intr().pi = 1;
    N64System::check_interrupt();
    Utils::debug("DMA Read completed");
}

PI PI::instance{};

} // namespace PI
} // namespace Mmio

Mmio::PI::PI &g_pi() { return Mmio::PI::PI::get_instance(); }

} // namespace N64

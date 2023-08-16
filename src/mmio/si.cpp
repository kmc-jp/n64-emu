#include "mmio/si.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "utils/utils.h"
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace SI {

constexpr uint32_t SI_DMA_DELAY = 65536 * 2;

void Pif::control_write() {
    Utils::debug("PIF: control_write");
    Utils::debug("{}", ram[63]);
    if (ram[63] > 1) {
        Utils::unimplemented("PIF_RAM[63] > 1");
        // TODO: what should be done here?
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PifRamHandler.cpp#L377
    }

    // https://github.com/SimoneN64/Kaizen/blob/74dccb6ac6a679acbf41b497151e08af6302b0e9/src/backend/core/mmio/PIF.cpp#L155
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PifRamHandler.cpp#L594
    uint8_t control = ram[63];
    int channel = 0;

    // For details of command,
    // see: https://n64brew.dev/wiki/Joybus_Protocol#Command_Details
    for (int cursor = 0; cursor < 64; cursor++) {
        Utils::debug("Joybus: channel = {}, cursor = {}, ram[cursor] = ",
                     channel, cursor, ram[cursor]);
        switch (ram[cursor]) {
        case 0x00: {
            channel++;
            if (channel > 6)
                cursor = 0x40;
        } break;
        case 0xFD: // fallthrough
        case 0xFE: {
            cursor = 0x40;
        } break;
        case 0xFF: // fallthrough
        case 0xB4: // fallthrough
        case 0x56: // fallthrough
        case 0xB8:
            break;
        default: {
            if (channel < 4) {
                process_controller_command(channel, &ram[cursor]);
            } else {
                Utils::critical("PIF: channel = {} >= 4", channel);
                Utils::unimplemented("Aborted");
            }
            // cursor advances by command length + command result length
            cursor += ram[cursor] + (ram[cursor] & 0x3F) + 1;
            channel++;
        } break;
        }
    }
}

void Pif::process_controller_command(int channel, uint8_t *cmd) {
    switch (cmd[2]) {
    case 0x00: // Info. fallthrough
    case 0xFF: // Reset/Info
    {
        // TODO: Add more kinds of joypad

        // N64 Controller
        cmd[3] = 0x05;
        cmd[4] = 0x00;
        // Pak installed
        cmd[5] = 0x01;
    } break;
    default: {
        Utils::critical("Unknown controller command: {}", cmd[2]);
        Utils::abort("Aborted");
    } break;
    }
}

void SI::reset() {
    Utils::debug("Resetting SI");
    // TODO: reset registers

    // non-registers
    dma_busy = 0;
}

void SI::dma_from_pif_to_dram() {
    Utils::debug("SI: DMA from PIF to DRAM");
    Utils::debug("PIF_ADDR: {:#010x}, DRAM_ADDR: {:#10x}", reg_pif_addr,
                 reg_dram_addr);
    dma_busy = true;
    pif.control_write();
    // FIXME: Should use offset `SI_PIF_ADDR + i`?
    // Project64: just use i
    // Kaizen: use SI_PIF_ADDR + i
    for (int i = 0; i < 64; i++)
        g_memory().get_rdram()[reg_dram_addr + i] = pif.ram[i];
    // TODO: should use scheduler?
    dma_busy = false;
    Utils::debug("SI: DMA complete");
}

void SI::dma_from_dram_to_pif() {
    Utils::debug("SI: DMA from DRAM to PIF");
    Utils::debug("PIF_ADDR: {:#010x}, DRAM_ADDR: {:#10x}", reg_pif_addr,
                 reg_dram_addr);
    dma_busy = true;
    // FIXME: Should use offset `SI_PIF_ADDR + i`?
    // Project64: just use i
    // Kaizen: use SI_PIF_ADDR + i
    for (int i = 0; i < 64; i++) {
        // Utils::debug("i = {}", i);
        pif.ram[i] = g_memory().get_rdram()[reg_dram_addr + i];
    }
    pif.control_write();
    // TODO: should use scheduler?
    dma_busy = false;
    Utils::debug("SI: DMA complete");
}

uint32_t SI::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_SI_DRAM_ADDR: {
        return reg_dram_addr;
    } break;
    case PADDR_SI_PIF_AD_RD64B: // fallthrough
    case PADDR_SI_PIF_AD_WR64B:
        return reg_pif_addr;
    case PADDR_SI_STATUS: {
        uint32_t value = 0;
        value |= dma_busy ? 1 : 0;
        value |= 0 << 1;
        value |= 0 << 3;
        value |= g_mi().get_reg_intr().si << 12;
        return value;
    } break;
    default: {
        Utils::critical("SI: Read from paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void SI::write_paddr32(uint32_t paddr, uint32_t value) {
    // https://github.com/SimoneN64/Kaizen/blob/74dccb6ac6a679acbf41b497151e08af6302b0e9/src/backend/core/mmio/SI.cpp#L55
    switch (paddr) {
    case PADDR_SI_DRAM_ADDR: {
        reg_dram_addr = value & RDRAM_SIZE_MASK;
    } break;
    case PADDR_SI_PIF_AD_RD64B: {
        reg_pif_addr = value & 0x1FFFFFFF;
        dma_from_pif_to_dram();
    } break;
    case PADDR_SI_PIF_AD_WR64B: {
        reg_pif_addr = value & 0x1FFFFFFF;
        dma_from_dram_to_pif();
    } break;
    case PADDR_SI_STATUS: {
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/SerialInterfaceHandler.cpp#L98
        g_mi().get_reg_intr().si = 0;
        reg_status &= ~SiStatusFlags::INTERRUPT;
        N64System::check_interrupt();
    } break;
    default: {
        Utils::critical("SI: Write to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

SI SI::instance{};

} // namespace SI
} // namespace Mmio
} // namespace N64
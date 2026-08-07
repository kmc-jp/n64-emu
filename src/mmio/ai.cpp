#include "mmio/ai.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "n64_system/scheduler.h"
#include "utils/log.h"

namespace N64 {
namespace Mmio {
namespace AI {

// Default NTSC-ish DACRATE (~44.1 kHz) when unset.
constexpr uint32_t DEFAULT_DACRATE = 1103;

void AI::reset() {
    Utils::debug("Resetting AI");
    dma_addr[0] = dma_addr[1] = 0;
    dma_length[0] = dma_length[1] = 0;
    fifo_count = 0;
    next_dram_addr = 0;
    dma_enable = false;
    dacrate = DEFAULT_DACRATE;
    bitrate = 0;
    delayed_carry = false;
}

uint64_t AI::cycles_for_length(uint32_t length) const {
    // Sample rate ≈ VI_clock / (dacrate + 1); each sample is 4 bytes (stereo 16-bit).
    // Scheduler ticks in CPU cycles; approximate with (dacrate+1) CPU cycles per sample.
    const uint32_t rate = dacrate == 0 ? DEFAULT_DACRATE : dacrate;
    const uint32_t samples = length / 4;
    if (samples == 0) {
        return 1;
    }
    return static_cast<uint64_t>(samples) * (rate + 1);
}

void AI::start_next_dma() {
    if (fifo_count == 0) {
        return;
    }

    if (delayed_carry) {
        dma_addr[0] += 0x2000;
        delayed_carry = false;
    }

    // AI IRQ fires when a DMA transfer *starts* (n64brew).
    g_mi().get_reg_intr().ai = 1;
    N64System::check_interrupt();

    g_scheduler().set_timer(
        cycles_for_length(dma_length[0]),
        N64System::Event{&AIScheduler::on_dma_complete});
}

void AI::enqueue_dma(uint32_t length) {
    if (length == 0 || fifo_count >= 2) {
        return;
    }

    const bool was_idle = (fifo_count == 0);
    dma_addr[fifo_count] = next_dram_addr;
    dma_length[fifo_count] = length;
    fifo_count++;

    if (was_idle) {
        start_next_dma();
    }
}

uint32_t AI::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_AI_LENGTH:
    case PADDR_AI_DRAM_ADDR:
    case PADDR_AI_CONTROL:
    case PADDR_AI_DACRATE:
    case PADDR_AI_BITRATE:
        // Write-only registers mirror AI_LENGTH on read (n64brew).
        return fifo_count > 0 ? dma_length[0] : 0;

    case PADDR_AI_STATUS: {
        uint32_t status = 0;
        // Unused bit that reads as 1 on hardware.
        status |= 1u << 24;
        status |= 1u << 19;
        if (dma_enable) {
            status |= AiStatusFlags::ENABLED;
        }
        if (fifo_count >= 1) {
            status |= AiStatusFlags::BUSY;
        }
        if (fifo_count >= 2) {
            status |= AiStatusFlags::FULL | AiStatusFlags::FULL_LO;
        }
        return status;
    }

    default: {
        Utils::critical("AI: Read from paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void AI::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_AI_DRAM_ADDR:
        // Lower 24 bits, 8-byte aligned.
        next_dram_addr = value & 0x00FFFFF8;
        break;

    case PADDR_AI_LENGTH: {
        // Bits [17:3]; lower 3 bits always 0.
        const uint32_t length = value & 0x3FFF8;
        enqueue_dma(length);
    } break;

    case PADDR_AI_CONTROL:
        dma_enable = (value & 1) != 0;
        break;

    case PADDR_AI_STATUS: {
        // Write acknowledges AI interrupt.
        g_mi().get_reg_intr().ai = 0;
        N64System::check_interrupt();
    } break;

    case PADDR_AI_DACRATE:
        dacrate = value & 0x3FFF;
        break;

    case PADDR_AI_BITRATE:
        bitrate = value & 0xF;
        break;

    default: {
        Utils::critical("AI: Write to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void AIScheduler::on_dma_complete() {
    AI &ai = g_ai();
    if (ai.fifo_count == 0) {
        return;
    }

    // Delayed-carry: last sample ends exactly on an 8 KiB page boundary.
    const uint32_t end_addr = ai.dma_addr[0] + ai.dma_length[0];
    ai.delayed_carry = ((end_addr & 0x1FFF) == 0);

    // Pop current buffer.
    if (ai.fifo_count == 2) {
        ai.dma_addr[0] = ai.dma_addr[1];
        ai.dma_length[0] = ai.dma_length[1];
    }
    ai.fifo_count--;
    ai.dma_addr[1] = 0;
    ai.dma_length[1] = 0;

    if (ai.fifo_count > 0) {
        ai.start_next_dma();
    }
}

AI &AI::get_instance() { return instance; }

AI AI::instance{};

} // namespace AI
} // namespace Mmio

Mmio::AI::AI &g_ai() { return Mmio::AI::AI::get_instance(); }

} // namespace N64

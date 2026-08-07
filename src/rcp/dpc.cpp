#include "rcp/dpc.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "rcp/rsp.h"
#include "utils/byte_array.h"
#include "utils/log.h"

namespace N64 {
namespace Rdp {

// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/rdp.c#L31
static const int COMMAND_LENGTHS[64] = {
    2, 2, 2, 2, 2, 2, 2, 2, 8, 12, 24, 28, 24, 28, 40, 44,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,
    2, 2, 2, 2, 4, 4, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2};

constexpr uint8_t RDP_COMMAND_FULL_SYNC = 0x29;
constexpr uint32_t RDP_COMMAND_BUFFER_SIZE = 0x10000;

void Dpc::reset() {
    Utils::debug("Resetting DPC");
    start = 0;
    end = 0;
    current = 0;
    status.raw = 0;
    clock = 0;
    tmem = 0;
}

uint32_t Dpc::read_paddr32(uint32_t paddr) const {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/rdp.c#L96
    switch (paddr) {
    case PADDR_DPC_START:
        return start;
    case PADDR_DPC_END:
        return end;
    case PADDR_DPC_CURRENT:
        return current;
    case PADDR_DPC_STATUS:
        return status.raw;
    case PADDR_DPC_CLOCK:
        return clock;
    case PADDR_DPC_BUFBUSY:
        return status.cmd_busy;
    case PADDR_DPC_PIPEBUSY:
        return status.pipe_busy;
    case PADDR_DPC_TMEM:
        return tmem;
    default:
        Utils::abort("Unknown DPC read {:#010x}", paddr);
    }
}

void Dpc::write_paddr32(uint32_t paddr, uint32_t value) {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/rdp.c#L65
    switch (paddr) {
    case PADDR_DPC_START:
        if (!status.start_valid) {
            start = value & 0x00FFFFF8;
        }
        status.start_valid = 1;
        break;
    case PADDR_DPC_END:
        end = value & 0x00FFFFF8;
        status.start_valid = 0;
        run_command();
        break;
    case PADDR_DPC_CURRENT:
        break;
    case PADDR_DPC_STATUS:
        status_write(value);
        break;
    case PADDR_DPC_CLOCK:
    case PADDR_DPC_BUFBUSY:
    case PADDR_DPC_PIPEBUSY:
    case PADDR_DPC_TMEM:
        break;
    default:
        Utils::abort("Unknown DPC write {:#010x}", paddr);
    }
}

void Dpc::status_write(uint32_t value) {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/rdp.c#L281
    const bool clear_xbus = value & 0x1;
    const bool set_xbus = value & 0x2;
    const bool clear_freeze = value & 0x4;
    const bool set_freeze = value & 0x8;
    const bool clear_flush = value & 0x10;
    const bool set_flush = value & 0x20;

    if (clear_xbus && !set_xbus)
        status.xbus_dmem_dma = 0;
    if (set_xbus && !clear_xbus)
        status.xbus_dmem_dma = 1;

    bool unfrozen = false;
    if (clear_freeze && !set_freeze) {
        status.freeze = 0;
        unfrozen = true;
    }
    if (set_freeze && !clear_freeze)
        status.freeze = 1;

    if (clear_flush && !set_flush)
        status.flush = 0;
    if (set_flush && !clear_flush)
        status.flush = 1;

    if (value & 0x40)
        tmem = 0;
    if (value & 0x80)
        status.pipe_busy = 0;
    if (value & 0x100)
        status.cmd_busy = 0;
    if (value & 0x200)
        clock = 0;

    if (unfrozen)
        run_command();
}

void Dpc::run_command() {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/rdp.c#L245
    if (status.freeze)
        return;
    status.pipe_busy = 1;
    status.start_gclk = 1;
    if (end > current) {
        process_list();
    }
    status.cbuf_ready = 1;
}

void Dpc::process_list() {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/rdp.c#L148
    // Stub: walk the list, raise DP interrupt on FULL_SYNC.
    // Parallel-RDP enqueue is wired in a later commit.
    status.freeze = 1;

    const uint32_t cur = current & 0x00FFFFF8;
    const uint32_t en = end & 0x00FFFFF8;
    int display_list_length = static_cast<int>(en - cur);
    if (display_list_length <= 0) {
        status.freeze = 0;
        return;
    }

    static uint32_t cmd_buf[RDP_COMMAND_BUFFER_SIZE];
    static int leftover = 0;

    if (status.xbus_dmem_dma) {
        auto &dmem = g_rsp().get_sp_dmem();
        for (int i = 0; i < display_list_length; i += 4) {
            cmd_buf[leftover + (i >> 2)] =
                Utils::read_from_byte_array32(dmem, (cur + i) & 0xFFF);
        }
    } else {
        if (en > 0x7FFFFFF || cur > 0x7FFFFFF) {
            Utils::warn("DPC list past end of RDRAM");
            status.freeze = 0;
            return;
        }
        auto &rdram = g_memory().get_rdram();
        for (int i = 0; i < display_list_length; i += 4) {
            cmd_buf[leftover + (i >> 2)] =
                Utils::read_from_byte_array32(rdram, cur + i);
        }
    }

    int length_words = (display_list_length >> 2) + leftover;
    int buf_index = 0;
    bool processed_all = true;

    while (buf_index < length_words) {
        uint8_t command = (cmd_buf[buf_index] >> 24) & 0x3F;
        int command_length = COMMAND_LENGTHS[command];
        if ((buf_index + command_length) * 4 >
            display_list_length + leftover * 4) {
            leftover = length_words - buf_index;
            for (int i = 0; i < leftover; i++) {
                cmd_buf[i] = cmd_buf[buf_index + i];
            }
            processed_all = false;
            break;
        }

        // Command enqueue to Parallel-RDP happens in phase 2.
        if (command == RDP_COMMAND_FULL_SYNC) {
            status.pipe_busy = 0;
            status.start_gclk = 0;
            status.cbuf_ready = 0;
            g_mi().get_reg_intr().dp = 1;
            N64System::check_interrupt();
        }

        buf_index += command_length;
    }

    if (processed_all)
        leftover = 0;

    current = en;
    end = en;
    status.freeze = 0;
}

Dpc Dpc::instance{};

} // namespace Rdp

Rdp::Dpc &g_dpc() { return Rdp::Dpc::get_instance(); }

} // namespace N64

#ifndef RI_H
#define RI_H

#include "cpu/cpu.h"
#include <cstdint>

namespace N64 {
namespace Mmio {

// RDRAM Interface
// https://n64brew.dev/wiki/RDRAM_Interface
class RI {
  private:
    enum Reg {
        MODE = 0,
        CONFIG = 1,
        SELECT = 3,
        REFLESH = 4,
    };

    uint32_t reg[6];

  public:
    RI() {}

    void reset() {
        // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/RI.cpp#L9
        reg[Reg::MODE] = 0xE;
        reg[Reg::CONFIG] = 040;
        reg[Reg::SELECT] = 0x14;
        reg[Reg::REFLESH] = 0x63634;
    }

    uint32_t read_paddr32(uint32_t paddr) {
        switch (paddr) {
        case 0x0470'0000: // mode
        case 0x0470'0004: // config
        case 0x0470'000C: // select
        case 0x0470'0010: // reflesh
        {
            uint32_t reg_num = (paddr - 0x0470'0000) / 4;
            return reg[reg_num];
        } break;
        case 0x0470'0008: // current_load
        case 0x0470'0014: // latency
        default: {
            spdlog::critical("Unimplemented. Read from RI paddr = 0x{:x}",
                             (uint32_t)paddr);
            n64cpu.dump();
            exit(-1);
        } break;
        }
    }

    void write_paddr32(uint32_t paddr, uint32_t value) {
        switch (paddr) {
        case 0x0470'0000: // mode
        case 0x0470'0004: // config
        case 0x0470'000C: // select
        case 0x0470'0010: // reflesh
        {
            uint32_t reg_num = (paddr - 0x0470'0000) / 4;
            reg[reg_num] = value;
        } break;
        case 0x0470'0008: // current_load
        case 0x0470'0014: // latency
        default: {
            spdlog::critical("Unimplemented. Write to RI paddr = 0x{:x}",
                             (uint32_t)paddr);
            n64cpu.dump();
            exit(-1);
        } break;
        }
    }
};

} // namespace Mmio

extern Mmio::RI n64ri;

} // namespace N64

#endif

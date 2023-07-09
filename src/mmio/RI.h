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
    uint32_t regs[6];

  public:
    RI() {}

    void init() {
        // TODO: what should be done?
    }

    uint32_t read_paddr32(uint32_t paddr) {
        switch (paddr) {
        case 0x0470'0000: // mode
        case 0x0470'0004: // config
        case 0x0470'0008: // current_load
        case 0x0470'000C: // select
        case 0x0470'0010: // reflesh
        case 0x0470'0014: // latency
        {
            uint32_t reg_num = (paddr - 0x0470'0000) / 4;
            return regs[reg_num];
        } break;
        default: {
            // correct?
            spdlog::critical("Illegal?. Read from RI paddr = 0x{:x}",
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
        case 0x0470'0008: // current_load
        case 0x0470'000C: // select
        case 0x0470'0010: // reflesh
        case 0x0470'0014: // latency
        {
            uint32_t reg_num = (paddr - 0x0470'0000) / 4;
            regs[reg_num] = value;
        } break;
        default: {
            // correct?
            spdlog::critical("Illegal?. Write to RI paddr = 0x{:x}",
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

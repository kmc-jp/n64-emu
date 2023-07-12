#ifndef PI_H
#define PI_H

#include "utils.h"
#include <cstdint>
#include <spdlog/spdlog.h>

namespace N64 {
namespace Mmio {
namespace PI {

// https://n64brew.dev/wiki/Peripheral_Interface
const uint32_t PADDR_DRAM_ADDR = 0x04600000;
const uint32_t PADDR_CART_ADDR = 0x04600004;
const uint32_t PADDR_RD_LEN = 0x04600008;
const uint32_t PADDR_WR_LEN = 0x0460000C;
const uint32_t PADDR_PI_STATUS = 0x04600010;

class PI {
  private:
    uint32_t reg[9];

  public:
    PI() {}

    void reset() {}

    uint8_t *get_pointer_to_paddr32(uint32_t paddr) {
        switch (paddr) {
        case PADDR_DRAM_ADDR:
        case PADDR_CART_ADDR:
        case PADDR_RD_LEN:
        case PADDR_WR_LEN:
        case PADDR_PI_STATUS: {
            uint32_t reg_num = (paddr - 0x0460'0000) / 4;
            return reinterpret_cast<uint8_t *>(&reg[reg_num]);
        } break;
        default: {
            spdlog::critical("Unimplemented. Access to PI paddr = {:#010x}",
                             paddr);
            Utils::core_dump();
            exit(-1);
        } break;
        }
    }
};

} // namespace PI
} // namespace Mmio

extern Mmio::PI::PI n64pi;

} // namespace N64

#endif

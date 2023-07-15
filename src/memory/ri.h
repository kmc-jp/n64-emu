#ifndef ri_H
#define ri_H

#include "utils/utils.h"
#include <cstdint>
#include <spdlog/spdlog.h>

namespace N64 {
namespace Memory {

const uint32_t PADDR_RI_MODE = 0x04700000;
const uint32_t PADDR_RI_CONFIG = 0x04700004;
const uint32_t PADDR_RI_CURRENT_LOAD = 0x04700008;
const uint32_t PADDR_RI_SELECT = 0x0470000C;
const uint32_t PADDR_RI_REFRESH = 0x0470001;

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

    uint32_t mode;
    uint32_t config;
    uint32_t current_load;
    uint32_t select;
    uint32_t refresh;

  public:
    RI() {}

    void reset() {
        // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/RI.cpp#L9
        mode = 0xE;
        config = 040;
        select = 0x14;
        refresh = 0x63634;
    }

    uint32_t read_paddr32(uint32_t paddr) {
        switch (paddr) {
        case PADDR_RI_MODE: // mode
            return mode;
        case PADDR_RI_CONFIG: // config
            return config;
        case PADDR_RI_CURRENT_LOAD: // current_load
            return current_load;
        case PADDR_RI_SELECT: // select
            return select;
        case PADDR_RI_REFRESH: // refresh
            return refresh;
        default: {
            spdlog::critical("Unimplemented. Read from RI paddr = {:#010x}",
                             paddr);
            Utils::core_dump();
            exit(-1);
        } break;
        }
    }

    void write_paddr32(uint32_t paddr, uint32_t value) {
        switch (paddr) {
        case PADDR_RI_MODE: // mode
        {
            mode = value;
        } break;
        case PADDR_RI_CONFIG: // config
        {
            config = value;
        } break;
        case PADDR_RI_CURRENT_LOAD: // current_load
        {
            current_load = value;
        } break;
        case PADDR_RI_SELECT: // select
        {
            select = value;
        } break;
        case PADDR_RI_REFRESH: // refresh
        {
            refresh = value;
        } break;
        default: {
            spdlog::critical("Unimplemented. Write to RI paddr = {:#010x}",
                             paddr);
            Utils::core_dump();
            exit(-1);
        } break;
        }
    }
};

} // namespace Memory
} // namespace N64

#endif

#include "memory/ri.h"
#include "utils/utils.h"

namespace N64 {
namespace Memory {

void RI::reset() {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64mem.c#L10
    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/RI.cpp#L9
    reg_mode = 0xE;
    reg_config = 040;
    reg_select = 0x14;
    reg_refresh = 0x63634;
}

uint32_t RI::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_RI_MODE: // mode
        return reg_mode;
    case PADDR_RI_CONFIG: // config
        return reg_config;
    case PADDR_RI_CURRENT_LOAD: // current_load
        return reg_current_load;
    case PADDR_RI_SELECT: // select
        return reg_select;
    case PADDR_RI_REFRESH: // refresh
        return reg_refresh;
    default: {
        Utils::critical("Unimplemented. Read from RI paddr = {:#010x}", paddr);
        Utils::core_dump();
        exit(-1);
    } break;
    }
}

void RI::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_RI_MODE: // mode
    {
        reg_mode = value;
    } break;
    case PADDR_RI_CONFIG: // config
    {
        reg_config = value;
    } break;
    case PADDR_RI_CURRENT_LOAD: // current_load
    {
        reg_current_load = value;
    } break;
    case PADDR_RI_SELECT: // select
    {
        reg_select = value;
    } break;
    case PADDR_RI_REFRESH: // refresh
    {
        reg_refresh = value;
    } break;
    default: {
        Utils::critical("Unimplemented. Write to RI paddr = {:#010x}", paddr);
        Utils::core_dump();
        exit(-1);
    } break;
    }
}

} // namespace Memory
} // namespace N64

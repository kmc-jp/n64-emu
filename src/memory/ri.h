#ifndef ri_H
#define ri_H

#include "utils/utils.h"
#include <cstdint>

namespace N64 {
namespace Memory {

const uint32_t PADDR_RI_MODE = 0x04700000;
const uint32_t PADDR_RI_CONFIG = 0x04700004;
const uint32_t PADDR_RI_CURRENT_LOAD = 0x04700008;
const uint32_t PADDR_RI_SELECT = 0x0470000C;
const uint32_t PADDR_RI_REFRESH = 0x04700010;

// RDRAM Interface
// https://n64brew.dev/wiki/RDRAM_Interface
class RI {
  private:
    uint32_t reg_mode;
    uint32_t reg_config;
    uint32_t reg_current_load;
    uint32_t reg_select;
    uint32_t reg_refresh;

  public:
    RI() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);
};

} // namespace Memory
} // namespace N64

#endif

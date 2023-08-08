#ifndef MI_H
#define MI_H

#include "memory/memory_map.h"
#include "utils/utils.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace MI {

const uint32_t PADDR_MI_MODE = 0x04300000;
const uint32_t PADDR_MI_VERSION = 0x04300004;
const uint32_t PADDR_MI_INTERRUPT = 0x04300008;
const uint32_t PADDR_MI_MASK = 0x0430000c;

// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/system/n64system.h#L64
// TODO: big endian?
typedef union mi_intr {
    uint32_t raw;
    PACK(struct {
        unsigned sp : 1;
        unsigned si : 1;
        unsigned ai : 1;
        unsigned vi : 1;
        unsigned pi : 1;
        unsigned dp : 1;
        unsigned : 26;
    });
} mi_intr_t;

static_assert(sizeof(mi_intr_t) == 4, "mi_intr_t size is not 4 bytes");

// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/system/n64system.h#L51
// TODO: big endian?
// FIXME: do not use bit field
typedef union mi_intr_mask {
    uint32_t raw;
    PACK(struct {
        unsigned sp : 1;
        unsigned si : 1;
        unsigned ai : 1;
        unsigned vi : 1;
        unsigned pi : 1;
        unsigned dp : 1;
        unsigned : 26;
    });
} mi_intr_mask_t;

static_assert(sizeof(mi_intr_mask_t) == 4,
              "mi_intr_mask_t size is not 4 bytes");

namespace MiModeFlag {
enum MiModeFlag : uint32_t {
    INIT = 0x0080,  // Bit  7: Initialization mode
    EBUS = 0x0100,  // Bit  8: EBUS test mode
    RDRAM = 0x0200, // Bit  9: RDRAM register mode
};
}

namespace MiModeWriteFlag {
enum MiModeWriteFlag : uint32_t {
    CLR_INIT = 0x0080,    // Bit  7: Clear initialization mode
    SET_INIT = 0x0100,    // Bit  8: Set initialization mode
    CLR_EBUS = 0x0200,    // Bit  9: Clear EBUS test
    SET_EBUS = 0x0400,    // Bit 10: Set EBUS test mode
    CLR_DP_INTR = 0x0800, // Bit 11: Clear DP interrupt
    CLR_RDRAM = 0x1000,   // Bit 12: Clear RDRAM register
    SET_RDRAM = 0x2000,   // Bit 13: Set RDRAM register mode
};
}

// MIPS Interface
// https://n64brew.dev/wiki/MIPS_Interface
class MI {
  private:
    uint32_t reg_mode;
    uint32_t reg_version;
    mi_intr_t reg_intr;
    mi_intr_mask_t reg_intr_mask;

    static MI instance;

  public:
    MI() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    mi_intr_t &get_reg_intr() { return reg_intr; }

    mi_intr_mask_t &get_reg_intr_mask() { return reg_intr_mask; }

    inline static MI &get_instance() { return instance; }
};

} // namespace MI
} // namespace Mmio

inline Mmio::MI::MI &g_mi() { return Mmio::MI::MI::get_instance(); }

} // namespace N64

#endif
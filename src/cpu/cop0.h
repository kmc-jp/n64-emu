#ifndef CPU_COP0_H
#define CPU_COP0_H

#include <spdlog/spdlog.h>

namespace N64 {

enum Cop0Reg {
    INDEX = 0,
    RANDOM = 1,
    STATUS = 12,
    PRID = 15,
    CONFIG = 16,
};

class Cop0 {
  public:
    // COP0 registers
    // ref: https://n64.readthedocs.io/#cop0-registers
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.h#L484
    uint64_t reg[32];

    /* 一つずつレジスタを定義するとメンドイのでやめる
    uint32_t index;
    uint32_t random;
    uint64_t entry_lo0; // TODO: refine type?
    uint64_t entry_lo1; // TODO: refine type?
    uint64_t context;   // TODO: refine type?
    uint32_t page_mask; // TODO: refine type?
    uint32_t wired;
    // 7th register is unknown
    uint64_t bad_vaddr;
    uint32_t count;
    uint64_t entry_hi; // TODO: refine type?
    uint32_t compare;
    uint64_t status; // TODO: refine type?
    uint64_t cause;  // TODO: refine type?F
    uint64_t epc;
    uint32_t prid;
    uint32_t config;
    uint32_t lladdr;
    uint32_t watch_lo; // TODO: refine type?
    uint32_t watch_hi;
    uint64_t xcontext; // TODO: refine type?
    // 21st register is unknown
    // 22st register is unknown
    // 23st register is unknown
    // 24st register is unknown
    // 25st register is unknown
    uint32_t parity_error;
    uint32_t cache_error;
    uint32_t tag_lo;
    uint32_t tag_hi;
    uint64_t error_epc;
    // 31st regiser is unknwon
    */

    Cop0() {}

    void init() {
        // TODO: what should be done?
        spdlog::debug("initializing CPU COP0");
    }
};

} // namespace N64

#endif

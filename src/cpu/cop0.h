#ifndef CPU_COP0_H
#define CPU_COP0_H

#include <spdlog/spdlog.h>

namespace N64 {
namespace Cpu {

enum Cop0Reg {
    INDEX = 0,
    RANDOM = 1,
    ENTRY_LO0 = 2,
    ENTRY_LO1 = 3,
    CONTEXT = 4,
    PAGE_MASK = 5,
    WIRED = 6,
    BAD_VADDR = 8,
    COUNT = 9,
    ENTRY_HI = 10,
    COMPARE = 11,
    STATUS = 12,
    CAUSE = 13,
    EPC = 14,
    PRID = 15,
    CONFIG = 16,
    LL_ADDR = 17,
    WATCH_LO = 18,
    WATCH_HI = 19,
    X_CONTEXT = 20,
    PARITY_ERROR = 26,
    CACHE_ERROR = 27,
    TAG_LO = 28,
    TAG_HI = 29,
    ERROR_EPC = 30,
};

// FIXME: packedいる?
typedef union cp0_cause {
    uint32_t raw;
    struct {
        unsigned : 8;
        unsigned interrupt_pending : 8;
        unsigned : 16;
    };
    struct {
        unsigned : 2;
        unsigned exception_code : 5;
        unsigned : 1;
        unsigned ip0 : 1;
        unsigned ip1 : 1;
        unsigned ip2 : 1;
        unsigned ip3 : 1;
        unsigned ip4 : 1;
        unsigned ip5 : 1;
        unsigned ip6 : 1;
        unsigned ip7 : 1;
        unsigned : 12;
        unsigned coprocessor_error : 2;
        unsigned : 1;
        unsigned branch_delay : 1;
    };
} cop0_cause_t;

// FIXME: packedいる?
typedef union cp0_status {
    uint32_t raw;
    struct {
        unsigned ie : 1;
        unsigned exl : 1;
        unsigned erl : 1;
        unsigned ksu : 2;
        unsigned ux : 1;
        unsigned sx : 1;
        unsigned kx : 1;
        unsigned im : 8;
        unsigned ds : 9;
        unsigned re : 1;
        unsigned fr : 1;
        unsigned rp : 1;
        unsigned cu0 : 1;
        unsigned cu1 : 1;
        unsigned cu2 : 1;
        unsigned cu3 : 1;
    };
    struct {
        unsigned : 16;
        unsigned de : 1;
        unsigned ce : 1;
        unsigned ch : 1;
        unsigned : 1;
        unsigned sr : 1;
        unsigned ts : 1;
        unsigned bev : 1;
        unsigned : 1;
        unsigned its : 1;
        unsigned : 7;
    };
} cop0_status_t;

class Cop0 {
  public:
    // COP0 registers
    // https://n64.readthedocs.io/#cop0-registers
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

    void reset() {
        spdlog::debug("initializing CPU COP0");
        // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/registers/Cop0.cpp#L11
        reg[Cop0Reg::CAUSE] = 0xB000007C;
        reg[Cop0Reg::STATUS] = 0;
        get_status()->cu0 = 1;
        get_status()->cu1 = 1;
        get_status()->fr = 1;
        reg[Cop0Reg::PRID] = 0x00000B22;
        reg[Cop0Reg::CONFIG] = 0x7006E463;
        reg[Cop0Reg::EPC] = 0xFFFFFFFFFFFFFFFFll;
        reg[Cop0Reg::ERROR_EPC] = 0xFFFFFFFFFFFFFFFFll;
        reg[Cop0Reg::WIRED] = 0;
        reg[Cop0Reg::INDEX] = 63;
        reg[Cop0Reg::BAD_VADDR] = 0xFFFFFFFFFFFFFFFF;
    }

    void dump() {
        for (int i = 0; i < 16; i++) {
            spdlog::info("CP0[{}]\t= 0x{:016x}\tCP0[{}]\t= 0x{:016x}", i,
                         reg[i], i + 16, reg[i + 16]);
        }
    }

    cop0_cause_t *get_cause() { return (cop0_cause_t *)&reg[Cop0Reg::CAUSE]; }

    cop0_status_t *get_status() {
        return (cop0_status_t *)&reg[Cop0Reg::STATUS];
    }

    uint8_t get_interrupt_pending_masked() {
        return get_cause()->interrupt_pending & get_status()->im;
    }
};

} // namespace Cpu
} // namespace N64

#endif

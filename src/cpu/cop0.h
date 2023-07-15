#ifndef CPU_COP0_H
#define CPU_COP0_H

#include <spdlog/spdlog.h>

namespace N64 {
namespace Cpu {

namespace Cop0Reg {
enum {
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
}

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
    // レジスタ幅については以下の1.3を参照
    // https://ultra64.ca/files/documentation/silicon-graphics/SGI_R4300_RISC_Processor_Specification_REV2.2.pdf

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

    Cop0() = default;

    void reset();

    void dump();

    cop0_cause_t *get_cause_ref() {
        return reinterpret_cast<cop0_cause_t *>(&reg[Cop0Reg::CAUSE]);
    }

    cop0_status_t *get_status_ref() {
        return reinterpret_cast<cop0_status_t *>(&reg[Cop0Reg::STATUS]);
    }

    uint8_t get_interrupt_pending_masked() {
        return get_cause_ref()->interrupt_pending & get_status_ref()->im;
    }

    uint32_t read32(uint32_t reg_num) const;
    uint64_t read64(uint32_t reg_num) const;

    void write32(uint32_t reg_num, uint32_t value);
    void write64(uint32_t reg_num, uint64_t value);

    template <typename Wire> Wire read(uint32_t reg_num) {
        static_assert(std::is_same_v<Wire, uint32_t>::value ||
                      std::is_same_v<Wire, uint64_t>::value);
        if constexpr (std::is_same_v<Wire, uint32_t>)
            return read32(reg_num);
        else
            return read64(reg_num);
    }

    template <typename Wire> void write(uint32_t reg_num, Wire value) {
        static_assert(std::is_same_v<Wire, uint32_t>::value ||
                      std::is_same_v<Wire, uint64_t>::value);
        if constexpr (std::is_same_v<Wire, uint32_t>)
            write32(reg_num, value);
        else
            write64(reg_num, value);
    }

  private:
    std::array<uint64_t, 32> reg{};
};

} // namespace Cpu
} // namespace N64

#endif
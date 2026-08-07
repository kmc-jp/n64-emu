#ifndef CPU_COP0_H
#define CPU_COP0_H

#include "utils/pack.h"
#include <array>
#include <cstdint>
#include <string_view>

namespace N64 {

union entry_lo0_t {
    uint32_t raw;
    PACK(struct {
        unsigned global : 1;
        unsigned v : 1;
        unsigned d : 1;
        unsigned c : 3;
        unsigned pfn : 20;
        unsigned : 6;
    });
};

static_assert(sizeof(entry_lo0_t) == 4);

union entry_lo1_t {
    uint32_t raw;
    PACK(struct {
        unsigned global : 1;
        unsigned v : 1;
        unsigned d : 1;
        unsigned c : 3;
        unsigned pfn : 20;
        unsigned : 6;
    });
};

static_assert(sizeof(entry_lo1_t) == 4);

// R4300 EntryHi is 64-bit: ASID / VPN2(27) / R(VA[63:62]). No G bit here.
PACK(union entry_hi_t {
    uint64_t raw;
    PACK(struct {
        uint64_t asid : 8;
        uint64_t : 5;
        uint64_t vpn2 : 27;
        uint64_t fill : 22;
        uint64_t r : 2;
    });
});

static_assert(sizeof(entry_hi_t) == 8);

constexpr uint64_t CP0_ENTRY_HI_WRITE_MASK = 0xC00000FFFFFFE0FFULL;
constexpr uint32_t CP0_ENTRY_LO_WRITE_MASK = 0x3FFFFFFF;
constexpr uint32_t CP0_PAGEMASK_WRITE_MASK = 0x1FFE000;

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
constexpr std::string_view UNUSED_COP0_REG_NAME = "unused";

constexpr std::array<std::string_view, 32> COP0_REG_NAMES = {
    "Index",
    "Random",
    "EntryLo0",
    "EntryLo1",
    "Context",
    "PageMask",
    "Wired",
    UNUSED_COP0_REG_NAME,
    "BadVAddr",
    "Count",
    "EntryHi",
    "Compare",
    "Status",
    "Cause",
    "EPC",
    "PRId",
    "Config",
    "LLAddr",
    "WatchLo",
    "WatchHi",
    "XContext",
    UNUSED_COP0_REG_NAME,
    UNUSED_COP0_REG_NAME,
    UNUSED_COP0_REG_NAME,
    UNUSED_COP0_REG_NAME,
    UNUSED_COP0_REG_NAME,
    "ECC",
    "CacheErr",
    "TagLo",
    "TagHi",
    "ErrEPC",
    UNUSED_COP0_REG_NAME,
};

// FIXME: bit fieldの順番があってるか確認
union cop0_cause_t {
    uint32_t raw;
    PACK(struct {
        unsigned : 8;
        unsigned interrupt_pending : 8;
        unsigned : 16;
    });
    PACK(struct {
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
    });
};

static_assert(sizeof(cop0_cause_t) == 4, "cop0_cause_t must be 32bit");

// Context / XContext: manipulate via masks to avoid cross-word bitfield issues.
union cop0_context_t {
    uint64_t raw;
};

static_assert(sizeof(cop0_context_t) == 8, "cop0_context_t must be 64bit");

union cop0_x_context_t {
    uint64_t raw;
};

static_assert(sizeof(cop0_x_context_t) == 8, "cop0_x_context_t must be 64bit");

// FIXME: bit fieldの順番があってるか確認
union cop0_status_t {
    uint32_t raw;
    PACK(struct {
        unsigned ie : 1;
        unsigned exl : 1;
        unsigned erl : 1;
        unsigned ksu : 2;
        unsigned ux : 1;
        unsigned sx : 1;
        unsigned kx : 1;
        unsigned im : 8; // interrupt mask
        unsigned ds : 9;
        unsigned re : 1;
        unsigned fr : 1;
        unsigned rp : 1;
        unsigned cu0 : 1;
        unsigned cu1 : 1;
        unsigned cu2 : 1;
        unsigned cu3 : 1;
    });
    struct PACK({
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
    });
};

static_assert(sizeof(cop0_status_t) == 4, "cop0_status_t must be 32bit");

class Cop0 {
    class Reg {
      public:
        // COP0 registers
        // The on-chip system control coprocessor (CP0) uses 25 registers.
        // These registers are 32 bits wide except for EntryHi, XContext,
        // EPC, and ErrorPC, which are 64 bits wide.
        // https://ultra64.ca/files/documentation/silicon-graphics/SGI_R4300_RISC_Processor_Specification_REV2.2.pdf
        uint32_t index;
        uint32_t random;
        entry_lo0_t entry_lo0;
        entry_lo1_t entry_lo1;
        cop0_context_t context;
        uint32_t page_mask;
        uint32_t wired;
        // 7th register is unknown
        uint64_t bad_vaddr;
        uint32_t count;
        entry_hi_t entry_hi;
        uint32_t compare;
        cop0_status_t status;
        cop0_cause_t cause;
        uint64_t epc;
        uint32_t prid;
        uint32_t config;
        uint32_t lladdr;
        uint32_t watch_lo;
        uint32_t watch_hi;
        cop0_x_context_t xcontext;
        // 21st register is unknown
        // 22st register is unknown
        // 23st register is unknown
        // 24st register is unknown
        // 25st register is unknown
        uint32_t parity_error;
        uint32_t cache_error;
        uint32_t tag_lo;
        uint32_t tag_hi;
        uint64_t error_epc; // 64bit
        // 31st regiser is unknwon

        uint64_t read(uint8_t reg_num) const;
        void write(uint8_t reg_num, uint64_t value);
    };

  public:
    Reg reg;
    // Set true iff LLAddr is set
    bool llbit;

    Cop0() {}

    void reset();

    void dump();

    // void on_status_updated();
};

} // namespace Cpu
} // namespace N64

#endif

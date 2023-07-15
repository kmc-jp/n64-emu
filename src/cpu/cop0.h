#ifndef CPU_COP0_H
#define CPU_COP0_H

#include "utils/utils.h"
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

typedef union cop0_x_context {
    uint64_t raw;
    struct {
        unsigned : 4;
        unsigned badvpn2 : 27;
        unsigned r : 2;
        unsigned ptebase : 31;
    };
} cop0_x_context_t;

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
    class Reg {
      public:
        // COP0 registers
        // > The on-chip system control coprocessor (CP0) uses 25 registers.
        // These > registers are 32 bits wide > except for EntryHi, XContext,
        // EPC, and > ErrorPC, which are 64 bits wide. 以下の1.3を参照
        // https://ultra64.ca/files/documentation/silicon-graphics/SGI_R4300_RISC_Processor_Specification_REV2.2.pdf
        uint32_t index;
        uint32_t random;
        uint32_t entry_lo0; // TODO: refine type?
        uint32_t entry_lo1; // TODO: refine type?
        uint32_t context;   // TODO: refine type?
        uint32_t page_mask; // TODO: refine type?
        uint32_t wired;
        // 7th register is unknown
        uint32_t bad_vaddr;
        uint32_t count;
        uint64_t entry_hi; // 64bit TODO: refine type?
        uint32_t compare;
        cop0_status_t status; // TODO: refine type?
        cop0_cause_t cause;   // TODO: refine type?F
        uint64_t epc;         // 64bit
        uint32_t prid;
        uint32_t config;
        uint32_t lladdr;
        uint32_t watch_lo; // TODO: refine type?
        uint32_t watch_hi;
        cop0_x_context_t xcontext; // 64bit TODO: refine type?
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

        uint64_t read(uint8_t reg_num) const {
            switch (reg_num) {
            case Cop0Reg::INDEX:
                return index;
            case Cop0Reg::RANDOM:
                return random;
            case Cop0Reg::ENTRY_LO0:
                return entry_lo0;
            case Cop0Reg::ENTRY_LO1:
                return entry_lo1;
            case Cop0Reg::CONTEXT:
                return context;
            case Cop0Reg::PAGE_MASK:
                return page_mask;
            case Cop0Reg::WIRED:
                return wired;
            case Cop0Reg::BAD_VADDR:
                return bad_vaddr;
            case Cop0Reg::COUNT:
                return count;
            case Cop0Reg::ENTRY_HI:
                return entry_hi;
            case Cop0Reg::COMPARE:
                return compare;
            case Cop0Reg::STATUS:
                return status.raw;
            case Cop0Reg::CAUSE:
                return cause.raw;
            case Cop0Reg::EPC:
                return epc;
            case Cop0Reg::PRID:
                return prid;
            case Cop0Reg::CONFIG:
                return config;
            case Cop0Reg::LL_ADDR:
                return lladdr;
            case Cop0Reg::WATCH_LO:
                return watch_lo;
            case Cop0Reg::WATCH_HI:
                return watch_hi;
            case Cop0Reg::X_CONTEXT:
                return xcontext.raw;
            case Cop0Reg::PARITY_ERROR:
                return parity_error;
            case Cop0Reg::CACHE_ERROR:
                return cache_error;
            case Cop0Reg::TAG_LO:
                return tag_lo;
            case Cop0Reg::TAG_HI:
                return tag_hi;
            case Cop0Reg::ERROR_EPC:
                return error_epc;
            default: {
                spdlog::info("Unimplemented; Access to COP0 {}th reg",
                             (uint32_t)reg_num);
                Utils::core_dump();
                exit(-1);
            } break;
            }
        }

        void write(uint8_t reg_num, uint64_t value) {
            switch (reg_num) {
            case Cop0Reg::INDEX: {
                index = value;
            } break;
            case Cop0Reg::RANDOM: {
                random = value;
            } break;
            case Cop0Reg::ENTRY_LO0: {
                entry_lo0 = value;
            } break;
            case Cop0Reg::ENTRY_LO1: {
                entry_lo1 = value;
            } break;
            case Cop0Reg::CONTEXT: {
                context = value;
            } break;
            case Cop0Reg::PAGE_MASK: {
                page_mask = value;
            } break;
            case Cop0Reg::WIRED: {
                wired = value;
            } break;
            case Cop0Reg::BAD_VADDR: {
                bad_vaddr = value;
            } break;
            case Cop0Reg::COUNT: {
                count = value;
            } break;
            case Cop0Reg::ENTRY_HI: {
                entry_hi = value;
            } break;
            case Cop0Reg::COMPARE: {
                compare = value;
            } break;
            case Cop0Reg::STATUS: {
                status.raw = value;
            } break;
            case Cop0Reg::CAUSE: {
                cause.raw = value;
            } break;
            case Cop0Reg::EPC: {
                epc = value;
            } break;
            case Cop0Reg::PRID: {
                prid = value;
            } break;
            case Cop0Reg::CONFIG: {
                config = value;
            } break;
            case Cop0Reg::LL_ADDR: {
                lladdr = value;
            } break;
            case Cop0Reg::WATCH_LO: {
                watch_lo = value;
            } break;
            case Cop0Reg::WATCH_HI: {
                watch_hi = value;
            } break;
            case Cop0Reg::X_CONTEXT: {
                xcontext.raw = value;
            } break;
            case Cop0Reg::PARITY_ERROR: {
                parity_error = value;
            } break;
            case Cop0Reg::CACHE_ERROR: {
                cache_error = value;
            } break;
            case Cop0Reg::TAG_LO: {
                tag_lo = value;
            } break;
            case Cop0Reg::TAG_HI: {
                tag_hi = value;
            } break;
            case Cop0Reg::ERROR_EPC: {
                error_epc = value;
            } break;
            default: {
                spdlog::info("Unimplemented; Access to COP0 {}th reg",
                             (uint32_t)reg_num);
                Utils::core_dump();
                exit(-1);
            } break;
            }
        }
    };

  public:
    Reg reg;

    Cop0() {}

    void reset() {
        spdlog::debug("initializing CPU COP0");
        // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/registers/Cop0.cpp#L11
        reg.cause.raw = 0xB000007C;
        reg.status.raw = 0;
        reg.status.cu0 = 1;
        reg.status.cu1 = 1;
        reg.status.fr = 1;
        reg.prid = 0x00000B22;
        reg.config = 0x7006E463;
        reg.epc = 0xFFFFFFFFFFFFFFFFll;
        reg.error_epc = 0xFFFFFFFFFFFFFFFFll;
        reg.wired = 0;
        reg.index = 63;
        reg.bad_vaddr = 0xFFFFFFFFFFFFFFFF;
    }

    void dump() {
        for (int i = 0; i < 16; i++) {
            bool i_th_reg_is_unknwon =
                i == 7 || (21 <= i && i <= 25) || i == 31;
            bool i_plus_16_th_reg_is_unknwon =
                (i + 16) == 7 || ((21 <= i + 16) && (i + 16 <= 25)) ||
                (i + 16) == 31;
            const uint64_t UNKNOWN_VAL = 0xccccdeadbeefcccc;
            spdlog::info(
                "CP0[{}]\t= {:#018x}\tCP0[{}]\t= {:#018x}", i,
                i_th_reg_is_unknwon ? UNKNOWN_VAL : reg.read(i), i + 16,
                i_plus_16_th_reg_is_unknwon ? UNKNOWN_VAL : reg.read(i + 16));
        }
        spdlog::info("global interrupt enabled; ie\t= {}",
                     reg.status.ie ? "enabled" : "disabled");
        spdlog::info("exception level; exl\t= {:d}", (uint32_t)reg.status.exl);
        spdlog::info("error level; erl\t= {:d}", (uint32_t)reg.status.erl);
        spdlog::info("execution mode; ksu\t= {:d}", (uint32_t)reg.status.ksu);
        spdlog::info("64bit addressing in user mode; ux\t= {}",
                     reg.status.ux ? "yes" : "no");
        spdlog::info("64bit addressing in supervisor mode; sx\t= {}",
                     reg.status.sx ? "yes" : "no");
        spdlog::info("64bit addressing in kernel mode; kx\t= {}",
                     reg.status.kx ? "yes" : "no");
        spdlog::info("interrupt mask; im\t= {:#010b}", (uint32_t)reg.status.im);
        // TODO: add more
        spdlog::info("cu0\t= {}\tcu2\t= {}",
                     reg.status.cu0 ? "enabled" : "disabled",
                     reg.status.cu2 ? "enabled" : "disabled");
        spdlog::info("cu1\t= {}\tcu3\t= {}",
                     reg.status.cu1 ? "enabled" : "disabled",
                     reg.status.cu3 ? "enabled" : "disabled");
    }
};

} // namespace Cpu
} // namespace N64

#endif

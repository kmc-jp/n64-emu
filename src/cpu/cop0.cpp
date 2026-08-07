#include "cpu/cop0.h"
#include "utils/log.h"

namespace N64 {
namespace Cpu {

namespace {
uint64_t se32_to_64(uint32_t value) {
    return static_cast<uint64_t>(static_cast<int32_t>(value));
}
} // namespace

uint64_t Cpu::Cop0::Reg::read(uint8_t reg_num) const {
    switch (reg_num) {
    case Cop0Reg::INDEX:
        return index;
    case Cop0Reg::RANDOM:
        return random;
    case Cop0Reg::ENTRY_LO0:
        return entry_lo0.raw;
    case Cop0Reg::ENTRY_LO1:
        return entry_lo1.raw;
    case Cop0Reg::CONTEXT:
        return context.raw;
    case Cop0Reg::PAGE_MASK:
        return page_mask;
    case Cop0Reg::WIRED:
        return wired;
    case Cop0Reg::BAD_VADDR:
        return bad_vaddr;
    case Cop0Reg::COUNT:
        return count;
    case Cop0Reg::ENTRY_HI:
        return entry_hi.raw;
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
        Utils::info("Unimplemented; Access to COP0 {} register",
                    COP0_REG_NAMES[reg_num]);
        Utils::abort("Aborted");
    } break;
    }
}

void Cpu::Cop0::Reg::write(uint8_t reg_num, uint64_t value) {
    switch (reg_num) {
    case Cop0Reg::INDEX: {
        index = value;
    } break;
    case Cop0Reg::RANDOM: {
        // Random is read-only on hardware; ignore software writes.
    } break;
    case Cop0Reg::ENTRY_LO0: {
        entry_lo0.raw = value & CP0_ENTRY_LO_WRITE_MASK;
    } break;
    case Cop0Reg::ENTRY_LO1: {
        entry_lo1.raw = value & CP0_ENTRY_LO_WRITE_MASK;
    } break;
    case Cop0Reg::CONTEXT: {
        // Software may only update PTEBase; preserve BadVPN2.
        const uint64_t v = (value > 0xFFFFFFFFULL)
                               ? value
                               : se32_to_64(static_cast<uint32_t>(value));
        context.raw =
            (v & 0xFFFFFFFFFF800000ULL) | (context.raw & 0x7FFFFFULL);
    } break;
    case Cop0Reg::PAGE_MASK: {
        page_mask = value & CP0_PAGEMASK_WRITE_MASK;
    } break;
    case Cop0Reg::WIRED: {
        wired = value & 0x3F;
        // Keep Random in Wired..31
        const uint32_t lo = (wired > 31) ? 0 : wired;
        const uint32_t hi = (wired > 31) ? 63 : 31;
        if (random < lo || random > hi) {
            random = lo;
        }
    } break;
    case Cop0Reg::BAD_VADDR: {
        // Read-only
    } break;
    case Cop0Reg::COUNT: {
        count = value;
    } break;
    case Cop0Reg::ENTRY_HI: {
        if (value <= 0xFFFFFFFFULL) {
            // MTC0: 32-bit write. Derive R from sign bit; VPN2 is 19 bits for
            // 32-bit addresses (do not let sign-extension pollute VPN2[26:19]).
            const uint32_t v = static_cast<uint32_t>(value);
            const uint64_t r = (static_cast<int32_t>(v) < 0) ? 0x3 : 0x0;
            const uint64_t vpn2 = (v >> 13) & 0x7FFFF;
            const uint64_t asid = v & 0xFF;
            entry_hi.raw = (r << 62) | (vpn2 << 13) | asid;
        } else {
            entry_hi.raw = value & CP0_ENTRY_HI_WRITE_MASK;
        }
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
        // Software may only update PTEBase; preserve BadVPN2 and R.
        const uint64_t v = (value > 0xFFFFFFFFULL)
                               ? value
                               : se32_to_64(static_cast<uint32_t>(value));
        xcontext.raw =
            (v & 0xFFFFFFFE00000000ULL) | (xcontext.raw & 0x1FFFFFFFFULL);
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
        Utils::info("Unimplemented; Access to COP0 {}th reg",
                    (uint32_t)reg_num);
        Utils::abort("Aborted");
    } break;
    }
}

void Cpu::Cop0::reset() {
    Utils::debug("Resetting CPU COP0");
    constexpr auto uint64_max = ~static_cast<uint64_t>(0);
    reg.cause.raw = 0xB000007C;
    reg.status.raw = 0;
    reg.status.cu0 = 1;
    reg.status.cu1 = 1;
    reg.status.fr = 1;
    reg.prid = 0x00000B22;
    reg.config = 0x7006E463;
    reg.epc = uint64_max;
    reg.error_epc = uint64_max;
    reg.wired = 0;
    reg.index = 63;
    reg.bad_vaddr = uint64_max;
    reg.context.raw = 0;
    reg.xcontext.raw = 0;
    reg.entry_hi.raw = 0;
    reg.random = 31;

    // FIXME: necessary?
    reg.cause.ip4 = 1;

    llbit = false;
}

void Cpu::Cop0::dump() {
    for (int i = 0; i < 16; i++) {
        bool i_th_reg_is_unknwon = COP0_REG_NAMES[i] == UNUSED_COP0_REG_NAME;
        bool i_plus_16_th_reg_is_unknwon =
            COP0_REG_NAMES[i + 16] == UNUSED_COP0_REG_NAME;
        const uint64_t UNKNOWN_VAL = 0xccccdeadbeefcccc;
        Utils::info("{}\t= {:#018x}\t{}\t= {:#018x}", COP0_REG_NAMES[i],
                    i_th_reg_is_unknwon ? UNKNOWN_VAL : reg.read(i),
                    COP0_REG_NAMES[i + 16],
                    i_plus_16_th_reg_is_unknwon ? UNKNOWN_VAL
                                                : reg.read(i + 16));
    }
    Utils::info("global interrupt enabled (ie) ? {}",
                reg.status.ie ? "Enabled" : "Disabled");
    Utils::info("interrupt mask (im) = {:#010b}", (uint32_t)reg.status.im);
    Utils::info(
        "interrupt is pending ? {}",
        reg.cause.interrupt_pending
            ? ((reg.cause.interrupt_pending & reg.status.im) ? "Yes" : "Masked")
            : "No");
    Utils::info("exception level (exl) = {:d}", (uint32_t)reg.status.exl);
    Utils::info("error level (erl) = {:d}", (uint32_t)reg.status.erl);
    Utils::info("execution mode (ksu) = {:d}", (uint32_t)reg.status.ksu);
    Utils::info("64bit addressing in user mode (ux) ? {}",
                reg.status.ux ? "Yes" : "No");
    Utils::info("64bit addressing in supervisor mode (sx) ? {}",
                reg.status.sx ? "Yes" : "No");
    Utils::info("64bit addressing in kernel mode (kx) ? {}",
                reg.status.kx ? "Yes" : "No");
    Utils::info("cu0 = {}\tcu2 = {}", reg.status.cu0 ? "Enabled" : "Disabled",
                reg.status.cu2 ? "Enabled" : "Disabled");
    Utils::info("cu1 = {}\tcu3 = {}", reg.status.cu1 ? "Enabled" : "Disabled",
                reg.status.cu3 ? "Enabled" : "Disabled");
}

} // namespace Cpu
} // namespace N64

﻿#include "cpu/cop0.h"
#include "utils/log.h"

namespace N64 {
namespace Cpu {

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
    // FIXME: Write to some registers should be ignored or masked.
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/Register.cpp#L367
    switch (reg_num) {
    case Cop0Reg::INDEX: {
        index = value;
    } break;
    case Cop0Reg::RANDOM: {
        random = value;
    } break;
    case Cop0Reg::ENTRY_LO0: {
        // FIXME: Correct?
        entry_lo0.raw = value & 0x3fff'ffff;
    } break;
    case Cop0Reg::ENTRY_LO1: {
        // FIXME: Correct?
        entry_lo1.raw = value & 0x3fff'ffff;
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
        // FIXME: should be masked with 0xC00000FFFFFFE0FF?
        entry_hi.raw = value;
    } break;
    case Cop0Reg::COMPARE: {
        compare = value;
    } break;
    case Cop0Reg::STATUS: {
        status.raw = value;
        // FIXME: should check interrupt?
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
        Utils::info("Unimplemented; Access to COP0 {}th reg",
                    (uint32_t)reg_num);
        Utils::abort("Aborted");
    } break;
    }
}

void Cpu::Cop0::reset() {
    Utils::debug("Resetting CPU COP0");
    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/registers/Cop0.cpp#L11
    constexpr auto uint64_max = ~static_cast<uint64_t>(0);
    constexpr auto uint32_max = ~static_cast<uint32_t>(0);
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
    reg.bad_vaddr = uint32_max;

    // FIXME: necessary?
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/N64System.cpp#L855
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

/*
void Cpu::Cop0::on_status_updated() {
    //
https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.h#L633
    bool exception = reg.status.exl || reg.status.erl;

    N64CPU.cp0.kernel_mode     =  exception || N64CPU.cp0.status.ksu ==
CPU_MODE_KERNEL; N64CPU.cp0.supervisor_mode = !exception &&
N64CPU.cp0.status.ksu == CPU_MODE_SUPERVISOR; N64CPU.cp0.user_mode       =
!exception && N64CPU.cp0.status.ksu == CPU_MODE_USER;
    N64CPU.cp0.is_64bit_addressing =
            (N64CPU.cp0.kernel_mode && N64CPU.cp0.status.kx)
            || (N64CPU.cp0.supervisor_mode && N64CPU.cp0.status.sx)
               || (N64CPU.cp0.user_mode && N64CPU.cp0.status.ux);
}
*/

} // namespace Cpu
} // namespace N64

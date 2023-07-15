#include "cop0.h"

namespace N64 {
namespace Cpu {

// TODO: read, writeそれぞれで有効なreg_numかチェック

void Cop0::reset() {
    spdlog::debug("initializing CPU COP0");
    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/registers/Cop0.cpp#L11
    reg[Cop0Reg::CAUSE] = 0xB000007C;
    reg[Cop0Reg::STATUS] = 0;
    get_status_ref()->cu0 = 1;
    get_status_ref()->cu1 = 1;
    get_status_ref()->fr = 1;
    reg[Cop0Reg::PRID] = 0x00000B22;
    reg[Cop0Reg::CONFIG] = 0x7006E463;
    reg[Cop0Reg::EPC] = 0xFFFFFFFFFFFFFFFFll;
    reg[Cop0Reg::ERROR_EPC] = 0xFFFFFFFFFFFFFFFFll;
    reg[Cop0Reg::WIRED] = 0;
    reg[Cop0Reg::INDEX] = 63;
    reg[Cop0Reg::BAD_VADDR] = 0xFFFFFFFFFFFFFFFF;
}

void Cop0::dump() {

    for (int i = 0; i < 16; i++) {
        spdlog::info("CP0[{}]\t= {:#018x}\tCP0[{}]\t= {:#018x}", i, reg[i],
                     i + 16, reg[i + 16]);
    }
    cop0_status_t *s = get_status_ref();
    spdlog::info("global interrupt enabled; ie\t= {}",
                 s->ie ? "enabled" : "disabled");
    spdlog::info("exception level; exl\t= {:d}", (uint32_t)s->exl);
    spdlog::info("error level; erl\t= {:d}", (uint32_t)s->erl);
    spdlog::info("execution mode; ksu\t= {:d}", (uint32_t)s->ksu);
    spdlog::info("64bit addressing in user mode; ux\t= {}",
                 s->ux ? "yes" : "no");
    spdlog::info("64bit addressing in supervisor mode; sx\t= {}",
                 s->sx ? "yes" : "no");
    spdlog::info("64bit addressing in kernel mode; kx\t= {}",
                 s->kx ? "yes" : "no");
    spdlog::info("interrupt mask; im\t= {:#010b}", (uint32_t)s->im);
    // TODO: add more
    spdlog::info("cu0\t= {}\tcu2\t= {}", s->cu0 ? "enabled" : "disabled",
                 s->cu2 ? "enabled" : "disabled");
    spdlog::info("cu1\t= {}\tcu3\t= {}", s->cu1 ? "enabled" : "disabled",
                 s->cu3 ? "enabled" : "disabled");
}

uint32_t Cop0::read32(uint32_t reg_num) const {
    assert(reg_num < 32);
    return static_cast<uint32_t>(reg[reg_num]);
}

uint64_t Cop0::read64(uint32_t reg_num) const {
    assert(reg_num < 32);
    return reg[reg_num];
}

void Cop0::write32(uint32_t reg_num, uint32_t value) {
    assert(reg_num < 32);
    reg[reg_num] = value;
}

void Cop0::write64(uint32_t reg_num, uint64_t value) {
    assert(reg_num < 32);
    reg[reg_num] = value;
}

} // namespace Cpu
} // namespace N64
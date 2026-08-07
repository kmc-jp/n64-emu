#include "cpu/cop1.h"
#include "utils/log.h"

namespace N64 {
namespace Cpu {

void Cop1::dump() {
    // TODO: Implement
}

void Cop1::reset() {
    Utils::debug("Resetting CPU COP1");
    fcr0 = FCR0_VR4300;
    fcr31.raw = 0;
    for (auto &reg : fgr) {
        reg.raw = 0;
    }
}

uint32_t Cop1::get_fgr_word(uint8_t reg, bool fr) const {
    if (fr) {
        return fgr[reg].lo;
    }
    if (reg & 1) {
        return fgr[reg & ~1].hi;
    }
    return fgr[reg].lo;
}

void Cop1::set_fgr_word(uint8_t reg, uint32_t value, bool fr) {
    if (fr) {
        fgr[reg].lo = value;
        return;
    }
    if (reg & 1) {
        fgr[reg & ~1].hi = value;
    } else {
        fgr[reg].lo = value;
    }
}

uint64_t Cop1::get_fgr_dword(uint8_t reg, bool fr) const {
    if (!fr) {
        reg &= ~1;
    }
    return fgr[reg].raw;
}

void Cop1::set_fgr_dword(uint8_t reg, uint64_t value, bool fr) {
    if (!fr) {
        reg &= ~1;
    }
    fgr[reg].raw = value;
}

} // namespace Cpu
} // namespace N64

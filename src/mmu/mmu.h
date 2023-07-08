#ifndef MMU_H
#define MMU_H

#include <spdlog/spdlog.h>

namespace N64 {
namespace Mmu {

// virtual memory map
// ref: https://n64.readthedocs.io/#virtual-memory-map
// clang-format off
const uint32_t KUSEG_BASE = 0x0;
const uint32_t KUSEG_END  = 0x7FFFFFFF;
const uint32_t KSEG0_BASE = 0x80000000;
const uint32_t KSEG0_END  = 0x9FFFFFFF;
const uint32_t KSEG1_BASE = 0xA0000000;
const uint32_t KSEG1_END  = 0xBFFFFFFF;
const uint32_t KSSEG_BASE = 0xC0000000;
const uint32_t KSSEG_END  = 0xDFFFFFFF;
const uint32_t KSEG3_BASE = 0xE0000000;
const uint32_t KSEG3_END  = 0xFFFFFFFF;
// clang-format on

/* virtual addressをphisical addressに変換する */
// ref: https://n64.readthedocs.io/#virtual-to-physical-address-translation
// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.h#L23
static uint32_t resolve_vaddr(uint32_t vaddr) {
    // FIXME: CPUモードによってアドレスが32bit長から64bit長になる

    if (KSEG0_BASE <= vaddr && vaddr <= KSEG0_END) {
        // KSEG0はdirect mapped. baseを引くだけでよい
        return vaddr - KSEG0_BASE;
    } else if (KSEG1_BASE <= vaddr && vaddr <= KSEG1_END) {
        // KSEG1はdirect mapped. baseを引くだけでよい
        return vaddr - KSEG1_BASE;
    } else {
        spdlog::debug("Unimplemented: address translation 0x{:x}", vaddr);
        exit(-1);
    }
}

} // namespace Mmu
} // namespace N64

#endif

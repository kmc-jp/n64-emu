#include "mmu.h"
#include "utils/utils.h"
#include <cstdint>

namespace N64 {
namespace Mmu {

// virtual addressをphisical addressに変換する
// ref: https://n64.readthedocs.io/#virtual-to-physical-address-translation
// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.h#L23
// TODO: manage cache?
uint32_t resolve_vaddr(uint32_t vaddr) {
    // FIXME: CPUモードによってアドレスが32bit長から64bit長になる
    uint32_t paddr;
    if (KSEG0_BASE <= vaddr && vaddr <= KSEG0_END) {
        // KSEG0はdirect mapped. baseを引くだけでよい
        paddr = vaddr - KSEG0_BASE;
    } else if (KSEG1_BASE <= vaddr && vaddr <= KSEG1_END) {
        // KSEG1はdirect mapped. baseを引くだけでよい
        paddr = vaddr - KSEG1_BASE;
    } else {
        Utils::debug("Unimplemented: address translation {:#010x}", vaddr);
        Utils::abort("Aborted");
    }
    Utils::trace("address translation vaddr {:#010x} => paddr {:#010x}", vaddr,
                 paddr);
    return paddr;
}

} // namespace Mmu
} // namespace N64

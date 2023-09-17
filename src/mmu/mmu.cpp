#include "mmu/mmu.h"
#include "mmu/tlb.h"
#include "utils/log.h"

namespace N64 {
namespace Mmu {

// Resolve virtual address to phisical address
// ref: https://n64.readthedocs.io/#virtual-to-physical-address-translation
// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.h#L23
// TODO: add more regions
std::optional<uint32_t> resolve_vaddr(uint32_t vaddr) {
    // FIXME: CPUモードによってアドレスが32bit長から64bit長になる
    uint32_t paddr;
    if (KSEG0_BASE <= vaddr && vaddr <= KSEG0_END) {
        // KSEG0 is direct mapped. substract vaddr with base
        paddr = vaddr - KSEG0_BASE;
    } else if (KSEG1_BASE <= vaddr && vaddr <= KSEG1_END) {
        // KSEG1 is direct mapped. subtract vaddr with base
        paddr = vaddr - KSEG1_BASE;
    } else if (KUSEG_BASE <= vaddr && vaddr <= KUSEG_END) {
        return g_tlb().probe(vaddr);
    } else {
        Utils::critical("address translation {:#010x}", vaddr);
        Utils::abort("Aborted");
    }
    Utils::trace("address translation vaddr {:#010x} => paddr {:#010x}", vaddr,
                 paddr);
    return {paddr};
}

} // namespace Mmu
} // namespace N64

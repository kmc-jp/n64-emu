#include "mmu/mmu.h"
#include "mmu/tlb.h"
#include "utils/log.h"

namespace N64 {
namespace Mmu {

std::optional<uint32_t> resolve_vaddr_slow(uint32_t vaddr,
                                           BusAccess bus_access) {
    // FIXME: address width is 32-bit or 64-bit depending on CPU mode
    if ((KUSEG_BASE <= vaddr && vaddr <= KUSEG_END) ||
        (KSSEG_BASE <= vaddr && vaddr <= KSSEG_END) ||
        (KSEG3_BASE <= vaddr && vaddr <= KSEG3_END)) {
        auto result = g_tlb().probe(vaddr, bus_access);
        if (!result.has_value()) {
            // Sign-extend 32-bit VA so EntryHi.R / XContext / is_xtlb_miss work.
            const uint64_t va64 =
                static_cast<uint64_t>(static_cast<int32_t>(vaddr));
            TLB::on_tlb_exception(va64);
        }
        return result;
    }
    Utils::critical("address translation {:#010x}", vaddr);
    Utils::abort("Aborted");
}

} // namespace Mmu
} // namespace N64

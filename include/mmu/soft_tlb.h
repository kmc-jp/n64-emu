#ifndef MMU_SOFT_TLB_H
#define MMU_SOFT_TLB_H

#include "mmu/tlb.h"
#include <cstdint>
#include <optional>

namespace N64 {
namespace Mmu {

// Direct-mapped VA page -> PA page cache for JIT / interpreter memory helpers.
// Invalidated on every TLBWI/TLBWR/reset. Only caches RDRAM-backed pages.
struct SoftTlbEntry {
    uint32_t vpn{0xFFFFFFFFu}; // vaddr >> 12; 0xFFFFFFFF = empty
    uint32_t pa_page{0};       // paddr & ~0xFFFu
};

constexpr uint32_t SOFT_TLB_BITS = 12;
constexpr uint32_t SOFT_TLB_SIZE = 1u << SOFT_TLB_BITS;
constexpr uint32_t SOFT_TLB_MASK = SOFT_TLB_SIZE - 1;

SoftTlbEntry *soft_tlb_load_table();
SoftTlbEntry *soft_tlb_store_table();

void soft_tlb_invalidate();

// Update after a successful resolve to RDRAM (page-aligned mapping).
void soft_tlb_note_load(uint32_t vaddr, uint32_t paddr);
void soft_tlb_note_store(uint32_t vaddr, uint32_t paddr);

// Soft-TLB hit for an access that stays within one 4 KiB page.
inline std::optional<uint32_t> soft_tlb_lookup(uint32_t vaddr,
                                               uint32_t access_size,
                                               bool is_store) {
    if (access_size == 0 ||
        (vaddr & 0xFFFu) + access_size - 1 > 0xFFFu)
        return std::nullopt;
    const uint32_t vpn = vaddr >> 12;
    const SoftTlbEntry &e =
        is_store ? soft_tlb_store_table()[vpn & SOFT_TLB_MASK]
                 : soft_tlb_load_table()[vpn & SOFT_TLB_MASK];
    if (e.vpn != vpn)
        return std::nullopt;
    return e.pa_page | (vaddr & 0xFFFu);
}

// Soft TLB then resolve_vaddr; notes RDRAM hits into the soft TLB.
std::optional<uint32_t> resolve_vaddr_cached(uint32_t vaddr,
                                             uint32_t access_size,
                                             BusAccess bus_access =
                                                 BusAccess::LOAD);

} // namespace Mmu
} // namespace N64

#endif

#ifndef MMU_SOFT_TLB_H
#define MMU_SOFT_TLB_H

#include <cstdint>

namespace N64 {
namespace Mmu {

// Direct-mapped VA page -> PA page cache for JIT / memory helpers.
// Invalidated on every TLBWI/TLBWR/reset. Only caches RDRAM-backed pages.
struct SoftTlbEntry {
    uint32_t vpn{0xFFFFFFFFu};     // vaddr >> 12; 0xFFFFFFFF = empty
    uint32_t pa_page{0};           // paddr & ~0xFFFu
};

constexpr uint32_t SOFT_TLB_BITS = 10;
constexpr uint32_t SOFT_TLB_SIZE = 1u << SOFT_TLB_BITS;
constexpr uint32_t SOFT_TLB_MASK = SOFT_TLB_SIZE - 1;

SoftTlbEntry *soft_tlb_load_table();
SoftTlbEntry *soft_tlb_store_table();

void soft_tlb_invalidate();

// Update after a successful resolve to RDRAM (page-aligned mapping).
void soft_tlb_note_load(uint32_t vaddr, uint32_t paddr);
void soft_tlb_note_store(uint32_t vaddr, uint32_t paddr);

} // namespace Mmu
} // namespace N64

#endif

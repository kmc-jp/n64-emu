#include "mmu/soft_tlb.h"
#include "memory/memory_map.h"
#include "mmu/mmu.h"
#include <array>

namespace N64 {
namespace Mmu {

namespace {
std::array<SoftTlbEntry, SOFT_TLB_SIZE> g_load{};
std::array<SoftTlbEntry, SOFT_TLB_SIZE> g_store{};

bool paddr_in_rdram(uint32_t paddr, uint32_t access_size) {
    return paddr <= PHYS_RDRAM_MEM_END &&
           paddr + (access_size - 1) <= PHYS_RDRAM_MEM_END;
}
} // namespace

SoftTlbEntry *soft_tlb_load_table() { return g_load.data(); }
SoftTlbEntry *soft_tlb_store_table() { return g_store.data(); }

void soft_tlb_invalidate() {
    for (auto &e : g_load)
        e.vpn = 0xFFFFFFFFu;
    for (auto &e : g_store)
        e.vpn = 0xFFFFFFFFu;
}

void soft_tlb_note_load(uint32_t vaddr, uint32_t paddr) {
    const uint32_t vpn = vaddr >> 12;
    SoftTlbEntry &e = g_load[vpn & SOFT_TLB_MASK];
    e.vpn = vpn;
    e.pa_page = paddr & ~0xFFFu;
}

void soft_tlb_note_store(uint32_t vaddr, uint32_t paddr) {
    const uint32_t vpn = vaddr >> 12;
    SoftTlbEntry &e = g_store[vpn & SOFT_TLB_MASK];
    e.vpn = vpn;
    e.pa_page = paddr & ~0xFFFu;
}

std::optional<uint32_t> resolve_vaddr_cached(uint32_t vaddr,
                                             uint32_t access_size,
                                             BusAccess bus_access) {
    const bool is_store = bus_access == BusAccess::STORE;
    if (auto hit = soft_tlb_lookup(vaddr, access_size, is_store))
        return hit;

    std::optional<uint32_t> paddr = resolve_vaddr(vaddr, bus_access);
    if (!paddr.has_value())
        return std::nullopt;

    const uint32_t p = paddr.value();
    if (paddr_in_rdram(p, access_size)) {
        if (is_store)
            soft_tlb_note_store(vaddr, p);
        else
            soft_tlb_note_load(vaddr, p);
    }
    return paddr;
}

} // namespace Mmu
} // namespace N64

#include "mmu/soft_tlb.h"
#include <array>

namespace N64 {
namespace Mmu {

namespace {
std::array<SoftTlbEntry, SOFT_TLB_SIZE> g_load{};
std::array<SoftTlbEntry, SOFT_TLB_SIZE> g_store{};
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

} // namespace Mmu
} // namespace N64

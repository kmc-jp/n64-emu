#ifndef MMU_H
#define MMU_H

#include "mmu/tlb.h"
#include <cstdint>
#include <optional>

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

// KSEG0/KSEG1 are direct-mapped (phys = vaddr & 0x1FFFFFFF). Hot-path inline.
inline std::optional<uint32_t> try_direct_map(uint32_t vaddr) {
    const uint32_t seg = vaddr >> 29;
    if (seg == 4 || seg == 5)
        return vaddr & 0x1FFFFFFFu;
    return std::nullopt;
}

// TLB / error path (not inlined into every caller).
std::optional<uint32_t> resolve_vaddr_slow(uint32_t vaddr,
                                           BusAccess bus_access = BusAccess::LOAD);

inline std::optional<uint32_t> resolve_vaddr(uint32_t vaddr,
                                             BusAccess bus_access =
                                                 BusAccess::LOAD) {
    if (auto p = try_direct_map(vaddr))
        return p;
    return resolve_vaddr_slow(vaddr, bus_access);
}

} // namespace Mmu
} // namespace N64

#endif

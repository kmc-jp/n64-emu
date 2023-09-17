#ifndef MMU_H
#define MMU_H

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

std::optional<uint32_t> resolve_vaddr(uint32_t vaddr);

} // namespace Mmu
} // namespace N64

#endif

#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <cstdint>

namespace N64 {

// physical memory map
// https://n64brew.dev/wiki/Memory_map
// clang-format off

// RDRAM with extension pack
constexpr uint32_t PHYS_RDRAM_MEM_BASE  = 0x00000000;
constexpr uint32_t PHYS_RDRAM_MEM_END   = 0x007FFFFF;

constexpr uint32_t PHYS_SPDMEM_BASE = 0x04000000;
constexpr uint32_t PHYS_SPDMEM_END  = 0x04000FFF;
constexpr uint32_t PHYS_SPIMEM_BASE = 0x04001000;
constexpr uint32_t PHYS_SPIMEM_END  = 0x04001FFF;

constexpr uint32_t PHYS_RSP_REG_BASE = 0x04040000;
constexpr uint32_t PHYS_RSP_REG_END = 0x040BFFFF;

constexpr uint32_t PHYS_MI_BASE     = 0x04300000;
constexpr uint32_t PHYS_MI_END      = 0x043FFFFF;
constexpr uint32_t PHYS_VI_BASE     = 0x04400000;
constexpr uint32_t PHYS_VI_END      = 0x044FFFFF;
constexpr uint32_t PHYS_AI_BASE     = 0x04500000;
constexpr uint32_t PHYS_AI_END      = 0x045FFFFF;
constexpr uint32_t PHYS_PI_BASE     = 0x04600000;
constexpr uint32_t PHYS_PI_END      = 0x046FFFFF;
constexpr uint32_t PHYS_RI_BASE     = 0x04700000;
constexpr uint32_t PHYS_RI_END      = 0x047FFFFF;
constexpr uint32_t PHYS_SI_BASE     = 0x04800000;
constexpr uint32_t PHYS_SI_END      = 0x048FFFFF;

constexpr uint32_t PHYS_ROM_BASE    = 0x10000000;
constexpr uint32_t PHYS_ROM_END     = 0x1FBFFFFF;

constexpr uint32_t PHYS_PIF_RAM_BASE = 0x1FC007C0;
constexpr uint32_t PHYS_PIF_RAM_END  = 0x1FC007FF;

// clang-format on

constexpr uint32_t RDRAM_SIZE = 0x00800000;
constexpr uint32_t RDRAM_SIZE_MASK = RDRAM_SIZE - 1;
constexpr uint32_t PIF_RAM_SIZE = 0x40;

} // namespace N64

#endif
#ifndef BUS_H
#define BUS_H

#include "memory.h"
#include "mmio/pi.h"
#include "mmio/si.h"
#include "rsp/rsp.h"
#include "utils/utils.h"

namespace N64 {
namespace Memory {

// physical memory map
// clang-format off
// RDRAM with extension pack
constexpr uint32_t PHYS_RDRAM_MEM_BASE  = 0x00000000;
constexpr uint32_t PHYS_RDRAM_MEM_END   = 0x03EFFFFF;

constexpr uint32_t PHYS_SPDMEM_BASE = 0x04000000;
constexpr uint32_t PHYS_SPDMEM_END  = 0x04000FFF;
constexpr uint32_t PHYS_SPIMEM_BASE = 0x04001000;
constexpr uint32_t PHYS_SPIMEM_END  = 0x04001FFF;

constexpr uint32_t PHYS_PI_BASE     = 0x04600000;
constexpr uint32_t PHYS_PI_END      = 0x046FFFFF;
constexpr uint32_t PHYS_RI_BASE     = 0x04700000;
constexpr uint32_t PHYS_RI_END      = 0x047FFFFF;

constexpr uint32_t PHYS_ROM_BASE    = 0x10000000;
constexpr uint32_t PHYS_ROM_END     = 0x1FBFFFFF;

constexpr uint32_t PHYS_PIF_RAM_BASE = 0x1FC007C0;
constexpr uint32_t PHYS_PIF_RAM_END  = 0x1FC007FF;
// TODO: もっと追加
// clang-format on

// 指定された物理アドレスから32bitを読み込む (big endian)
// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.c#L488
uint64_t read_paddr64(uint32_t paddr);
uint32_t read_paddr32(uint32_t paddr);
uint16_t read_paddr16(uint32_t paddr);

// 指定された物理アドレスに32bitを書き込む (big endian)
void write_paddr32(uint32_t paddr, uint32_t value);

} // namespace Memory
} // namespace N64

#endif
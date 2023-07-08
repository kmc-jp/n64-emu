#ifndef BUS_H
#define BUS_H

#include "../rsp/rsp.h"
#include "memory.h"
#include <spdlog/spdlog.h>


namespace N64 {

// physical memory map
// clang-format off
// RDRAM with extension pack
const uint32_t PHYS_RDRAM_BASE  = 0x00000000;
const uint32_t PHYS_RDRAM_END   = 0x007FFFFF;

const uint32_t PHYS_SPDMEM_BASE = 0x04000000;
const uint32_t PHYS_SPDMEM_END  = 0x04000FFF;

// TODO: もっと追加
// clang-format on

namespace Bus {

// 指定された物理アドレスから32bitを読み込む (big endian)
// ref:
// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.c#L488
static uint32_t read_paddr32(uint32_t paddr) {
    // アラインメントのチェック

    if (PHYS_RDRAM_BASE <= paddr && paddr <= PHYS_RDRAM_END) {
        uint32_t offs = paddr - PHYS_RDRAM_BASE;
        return (n64mem.rdram[offs + 0] << 24) + (n64mem.rdram[offs + 1] << 16) +
               (n64mem.rdram[offs + 2] << 8) + n64mem.rdram[offs + 3];
    } else if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
        uint32_t offs = paddr - PHYS_SPDMEM_BASE;
        spdlog::info("0x{:x}", offs);
        return (n64rsp.sp_dmem[offs + 0] << 24) +
               (n64rsp.sp_dmem[offs + 1] << 16) +
               (n64rsp.sp_dmem[offs + 2] << 8) + n64rsp.sp_dmem[offs + 3];
    } else {
        spdlog::critical("Unimplemented. read from paddr = 0x{:x}", paddr);
        exit(-1);
    }
}

} // namespace Bus
} // namespace N64

#endif

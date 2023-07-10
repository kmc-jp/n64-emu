#ifndef BUS_H
#define BUS_H

#include "memory.h"
#include "rsp/rsp.h"
#include "utils/utils.h"
#include <spdlog/spdlog.h>

namespace N64 {
namespace Memory {

// physical memory map
// clang-format off
// RDRAM with extension pack
const uint32_t PHYS_RDRAM_BASE  = 0x00000000;
const uint32_t PHYS_RDRAM_END   = 0x007FFFFF;
const uint32_t PHYS_SPDMEM_BASE = 0x04000000;
const uint32_t PHYS_SPDMEM_END  = 0x04000FFF;

const uint32_t PHYS_RI_BASE     = 0x04700000;
const uint32_t PHYS_RI_END      = 0x047FFFFF;
// TODO: もっと追加
// clang-format on

// 指定された物理アドレスから32bitを読み込む (big endian)
// ref:
// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.c#L488
static uint32_t read_paddr32(uint32_t paddr) {
    // TODO: アラインメントのチェック

    if (PHYS_RDRAM_BASE <= paddr && paddr <= PHYS_RDRAM_END) {
        uint32_t offs = paddr - PHYS_RDRAM_BASE;
        return Utils::read_from_byte_array32(&n64mem.rdram[offs]);
    } else if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
        uint32_t offs = paddr - PHYS_SPDMEM_BASE;
        return Utils::read_from_byte_array32(&n64rsp.sp_dmem[offs]);
    } else if (PHYS_RI_BASE <= paddr && paddr <= PHYS_RI_END) {
        return n64mem.ri.read_paddr32(paddr);
    } else {
        spdlog::critical("Unimplemented. read from paddr = 0x{:x}", paddr);
        Utils::core_dump();
        exit(-1);
    }
}

} // namespace Memory
} // namespace N64

#endif

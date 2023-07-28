#ifndef BUS_H
#define BUS_H

#include <cstdint>

namespace N64 {
namespace Memory {

// 指定された物理アドレスから32bitを読み込む (big endian)
// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.c#L488
uint64_t read_paddr64(uint32_t paddr);
uint32_t read_paddr32(uint32_t paddr);
uint16_t read_paddr16(uint32_t paddr);

// TODO: Memoryクラスに移動
// 指定された物理アドレスに32bitを書き込む (big endian)
void write_paddr32(uint32_t paddr, uint32_t value);

} // namespace Memory
} // namespace N64

#endif
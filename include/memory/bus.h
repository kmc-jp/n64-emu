#ifndef BUS_H
#define BUS_H

#include <cstdint>

namespace N64 {
namespace Memory {

// TODO: move to memory class?
// 指定された物理アドレスから32bitを読み込む (big endian)
// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.c#L488
uint64_t read_paddr64(uint32_t paddr);
uint32_t read_paddr32(uint32_t paddr);
uint16_t read_paddr16(uint32_t paddr);
uint8_t read_paddr8(uint32_t paddr);

// TODO: move to memory class?
// 指定された物理アドレスに32bitを書き込む (big endian)
void write_paddr64(uint32_t paddr, uint64_t value);
void write_paddr32(uint32_t paddr, uint32_t value);
void write_paddr16(uint32_t paddr, uint16_t value);
void write_paddr8(uint32_t paddr, uint8_t value);

} // namespace Memory
} // namespace N64

#endif
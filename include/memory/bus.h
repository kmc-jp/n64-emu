#ifndef BUS_H
#define BUS_H

#include <cstdint>

namespace N64 {
namespace Memory {

// TODO: move to memory class?
// Physical bus accessors. RDRAM/ROM/IMEM use host-endian word storage
uint64_t read_paddr64(uint32_t paddr);
uint32_t read_paddr32(uint32_t paddr);
uint16_t read_paddr16(uint32_t paddr);
uint8_t read_paddr8(uint32_t paddr);

// TODO: move to memory class?
void write_paddr64(uint32_t paddr, uint64_t value);
void write_paddr32(uint32_t paddr, uint32_t value);
void write_paddr16(uint32_t paddr, uint16_t value);
void write_paddr8(uint32_t paddr, uint8_t value);

} // namespace Memory
} // namespace N64

#endif

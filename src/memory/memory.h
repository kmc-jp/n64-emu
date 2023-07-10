#ifndef MEMORY_H
#define MEMORY_H

#include "ri.h"
#include "rom.h"
#include <cstdint>
#include <iostream>
#include <spdlog/spdlog.h>

namespace N64 {
namespace Memory {

// RDRAM with extension pack
const int RDRAM_SIZE = 0x800000;

class Memory {
  public:
    uint8_t rdram[RDRAM_SIZE];
    RI ri;
    Rom rom;

    Memory() {}

    void reset() { ri.reset(); }

    void load_rom(std::string rom_filepath) {
        spdlog::debug("initializing RDRAM");
        rom.read_from(rom_filepath);
    }
};

} // namespace Memory

extern Memory::Memory n64mem;

} // namespace N64

#endif
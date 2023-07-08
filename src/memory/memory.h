#ifndef MEMORY_H
#define MEMORY_H

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
    Rom rom;

    Memory() {}

    void init_with_rom(std::string rom_filepath) {
        spdlog::debug("initializing RDRAM");
        rom.init(rom_filepath);
    }
};

} // namespace Memory

extern Memory::Memory n64mem;

} // namespace N64

#endif
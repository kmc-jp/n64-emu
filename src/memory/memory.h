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

    void load_rom(const std::string &rom_filepath) {
        spdlog::debug("initializing RDRAM");
        rom.read_from(rom_filepath);
    }

    static Memory &get_instance() { return instance; }

  private:
    static Memory instance;
};

} // namespace Memory

inline Memory::Memory &g_memory() { return Memory::Memory::get_instance(); }

} // namespace N64

#endif
#ifndef MEMORY_H
#define MEMORY_H

#include "memory_map.h"
#include "ri.h"
#include "rom.h"
#include <cstdint>
#include <iostream>
#include <vector>

namespace N64 {
namespace Memory {

class Memory {
    std::vector<uint8_t> rdram;

  public:
    RI ri;
    Rom rom;

    Memory() : rdram({}) { rdram.assign(RDRAM_MEM_SIZE, 0); }

    void reset() {
        Utils::debug("Resetting Memory (RDRAM)");
        ri.reset();
    }

    void load_rom(const std::string &rom_filepath) {
        rom.load_file(rom_filepath);
    }

    static Memory &get_instance() { return instance; }

    std::vector<uint8_t> &get_rdram() { return rdram; }

  private:
    static Memory instance;
};

} // namespace Memory

inline Memory::Memory &g_memory() { return Memory::Memory::get_instance(); }

} // namespace N64

#endif
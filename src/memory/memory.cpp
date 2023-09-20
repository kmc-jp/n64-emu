#include "memory/memory.h"
#include "memory/memory_map.h"
#include <iostream>

namespace N64 {
namespace Memory {

Memory::Memory() : rdram({}) { rdram.assign(RDRAM_SIZE, 0); }

void Memory::reset() {
    Utils::debug("Resetting Memory (RDRAM)");
    ri.reset();
}

void Memory::load_rom(const std::string &rom_filepath) {
    rom.load_file(rom_filepath);
}

Memory &Memory::get_instance() { return instance; }

std::vector<uint8_t> &Memory::get_rdram() { return rdram; }

Memory Memory::instance{};

} // namespace Memory

Memory::Memory &g_memory() { return Memory::Memory::get_instance(); }

} // namespace N64

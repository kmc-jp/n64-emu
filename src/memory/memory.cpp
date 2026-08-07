#include "memory/memory.h"
#include "memory/memory_map.h"
#include "utils/log.h"
#include <iostream>

namespace N64 {
namespace Memory {

Memory::Memory() : rdram({}), sram({}) { rdram.assign(RDRAM_SIZE, 0); }

void Memory::reset() {
    Utils::debug("Resetting Memory (RDRAM)");
    ri.reset();
}

void Memory::load_rom(const std::string &rom_filepath) {
    rom.load_file(rom_filepath);
    allocate_sram();
}

void Memory::allocate_sram() {
    if (rom.get_save_type() == SaveType::Sram256k) {
        sram.assign(SRAM_SIZE, 0xFF);
    } else {
        sram.clear();
    }
}

Memory &Memory::get_instance() { return instance; }

std::vector<uint8_t> &Memory::get_rdram() { return rdram; }

std::vector<uint8_t> &Memory::get_sram() { return sram; }

Memory Memory::instance{};

} // namespace Memory

Memory::Memory &g_memory() { return Memory::Memory::get_instance(); }

} // namespace N64

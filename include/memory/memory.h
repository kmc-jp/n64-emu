#ifndef MEMORY_H
#define MEMORY_H

#include "ri.h"
#include "rom.h"
#include <cstdint>
#include <string>
#include <vector>

namespace N64 {
namespace Memory {

class Memory {
    std::vector<uint8_t> rdram;

  public:
    RI ri;
    Rom rom;

    Memory();

    void reset();

    void load_rom(const std::string &rom_filepath);

    static Memory &get_instance();

    std::vector<uint8_t> &get_rdram();

  private:
    static Memory instance;
};

} // namespace Memory

Memory::Memory &g_memory();

} // namespace N64

#endif

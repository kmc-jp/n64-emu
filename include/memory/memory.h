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
    std::vector<uint8_t> sram;
    std::string data_dir{"."};
    std::string sram_path;

  public:
    RI ri;
    Rom rom;

    Memory();

    void reset();

    // Root app folder (e.g. ~/.local/share/kamo64). Saves go under save/.
    void set_data_dir(const std::string &dir);

    void load_rom(const std::string &rom_filepath);

    // Write cartridge SRAM to disk if allocated (no-op otherwise).
    void persist_sram();

    static Memory &get_instance();

    std::vector<uint8_t> &get_rdram();

    std::vector<uint8_t> &get_sram();

  private:
    void allocate_sram();
    void load_sram_file();
    std::string cart_save_path(const char *filename) const;

    static Memory instance;
};

} // namespace Memory

Memory::Memory &g_memory();

} // namespace N64

#endif

#ifndef ROM_H
#define ROM_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vector>

namespace N64 {
namespace Memory {

typedef struct RomHeader {
    uint8_t initial_values[4];
    uint32_t clock_rate;
    uint32_t program_counter;
    uint32_t release;
    uint32_t crc1;
    uint32_t crc2;
    uint32_t unknown1;
    uint32_t unknown2;

    char image_name[20];
    uint32_t unknown5;
    uint32_t unknown6;
    uint32_t manufacturer_id;
    uint16_t cartridge_id;
    union {
        char country_code[2];
        uint16_t country_code_int;
    };
    uint8_t boot_code[4032];
} rom_header_t;

class Rom {
  private:
    // pointer to raw byte string
    std::vector<uint8_t> rom;
    rom_header_t header;
    bool broken;

  public:
    Rom() : rom({}), broken(true) {}

    void read_from(std::string filepath) {
        spdlog::debug("reading from ROM");

        std::ifstream file(filepath.c_str(), std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Could not open ROM file");
            return;
        }
        rom = {std::istreambuf_iterator<char>(file),
               std::istreambuf_iterator<char>()};

        if (rom.size() < sizeof(rom_header_t)) {
            spdlog::error("ROM is too small");
            return;
        }

        // set header
        header = *((rom_header_t *)rom.data());

        spdlog::debug("ROM size\t= {}", rom.size());
        spdlog::debug("imageName\t= \"{}\"", std::string(header.image_name));
        broken = false;
    }

    // bool is_broken() const { return broken; }

    // ROMの生データの先頭へのポインタを返す
    uint8_t *raw() const { return (uint8_t *)rom.data(); }
};

} // namespace Memory
} // namespace N64

#endif

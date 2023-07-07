#ifndef ROM_H
#define ROM_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

namespace N64 {

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

    void init(std::string filepath) {
        std::cout << "reading ROM" << std::endl;

        std::ifstream file(filepath.c_str(), std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            std::cout << "Could not open ROM file" << std::endl;
            return;
        }
        rom = {std::istreambuf_iterator<char>(file),
               std::istreambuf_iterator<char>()};

        if (rom.size() < sizeof(rom_header_t)) {
            std::cout << "ROM is too small" << std::endl;
            return;
        }

        // set header
        header = *((rom_header_t *)rom.data());

        std::cout << "ROM size\t= " << rom.size() << std::endl;
        std::cout << "ROM imageName\t= " << header.image_name << std::endl;
        std::cout << "ROM imageName\t= " << header.image_name << std::endl;
        broken = false;
    }

    bool is_broken() const { return broken; }

    /* ROMの先頭からのオフセットのデータを1byte取得する */
    uint8_t at(uint32_t offs) const { return rom.at(offs); }
};

} // namespace N64

#endif

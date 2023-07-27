#ifndef ROM_H
#define ROM_H

#include "utils.h"
#include <cstdint>
#include <vector>

namespace N64 {
namespace Memory {

// cartridge size
// https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/MemoryRegions.hpp#L18
constexpr uint32_t ROM_SIZE = 0xF000'0000;

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
} rom_header_t;

enum class CicType {
    CIC_UNKNOWN,
    CIC_NUS_6101,
    CIC_NUS_7102,
    CIC_NUS_6102_7101,
    CIC_NUS_6103_7103,
    CIC_NUS_6105_7105,
    CIC_NUS_6106_7106
};

class Rom {
  private:
    // `std::array` cannot be used because size of data is very large.
    std::vector<uint8_t> rom;
    rom_header_t header;
    CicType cic{};

  public:
    Rom() : rom({}) {
        rom.assign(ROM_SIZE, 0);
    }

    void load_file(const std::string &filepath);

    CicType get_cic() const { return cic; }

    // ROMの生データの先頭へのポインタを返す
    // FIXME: 生ポインタは使いたくない, read_offset8, read_offset32,
    // write_offset8, write_offset32を使う
    uint8_t *raw() { return reinterpret_cast<uint8_t *>(rom.data()); }

    uint8_t read_offset8(uint32_t offset) const { return rom.at(offset); }

    uint16_t read_offset16(uint32_t offset) const {
        return Utils::read_from_byte_array16(rom, offset);
    }

    uint32_t read_offset32(uint32_t offset) const {
        return Utils::read_from_byte_array32(rom, offset);
    }
};

} // namespace Memory
} // namespace N64

#endif
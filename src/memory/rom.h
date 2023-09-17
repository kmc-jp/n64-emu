#ifndef ROM_H
#define ROM_H

#include <cstdint>
#include <vector>

namespace N64 {
namespace Memory {

// cartridge size
// https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/MemoryRegions.hpp#L18
constexpr uint32_t ROM_SIZE = 0xF000'0000;

// https://www.romhacking.net/forum/index.php?topic=19524.0
// https://github.com/saneki/n64rom-rs/blob/757e34e039acd96a630348801a7435b16be59df0/src/header.rs#L143
struct rom_header_t {
    uint8_t initial_values[4];
    uint32_t clock_rate;
    uint32_t program_counter;
    uint32_t release;
    uint32_t crc1;
    uint32_t crc2;
    char unknown1[4];
    char unknown2[4];
    // 32 bit before here
    char image_name[20];
    // FIXME: これ以降はあっているか不明
    char unknown5[4];
    uint32_t manufacturer_id;
    uint16_t cartridge_id;
    union {
        char country_code[2];
        uint16_t country_code_int;
    };
};

static_assert(sizeof(rom_header_t) == 0x40,
              "rom_header_t size must be 0x40 bytes");

enum class RomType {
    Z64,
    N64,
    V64,
    UNKNOWN,
};

enum class CicType : uint32_t {
    CIC_UNKNOWN = 0,
    CIC_NUS_6101 = 1,
    CIC_NUS_7102 = 2,
    CIC_NUS_6102_7101 = 3,
    CIC_NUS_6103_7103 = 4,
    CIC_NUS_6105_7105 = 5,
    CIC_NUS_6106_7106 = 6
};

class Rom {
  private:
    // `std::array` cannot be used because size of data is very large.
    std::vector<uint8_t> rom;
    rom_header_t header;
    CicType cic{};

  public:
    Rom();

    void load_file(const std::string &filepath);

    RomType rom_type();

    CicType get_cic() const;

    uint32_t get_cic_seed() const;

    std::vector<uint8_t> &get_raw_data();

    uint8_t read_offset8(uint32_t offset) const;

    uint16_t read_offset16(uint32_t offset) const;

    uint32_t read_offset32(uint32_t offset) const;
};

} // namespace Memory
} // namespace N64

#endif

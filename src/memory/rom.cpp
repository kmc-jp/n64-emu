#include "rom.h"

namespace N64 {
namespace Memory {

constexpr int CRC32_TABLE_SIZE = 256;

constexpr std::array<uint32_t, CRC32_TABLE_SIZE> crc32_table() {
    std::array<uint32_t, CRC32_TABLE_SIZE> table{};
    for (int i = 0; i < CRC32_TABLE_SIZE; i++) {
        uint32_t rem = i;
        for (int j = 0; j < 8; j++) {
            if (rem & 1) {
                rem >>= 1;
                rem ^= 0xedb88320;
            } else
                rem >>= 1;
        }
        table[i] = rem;
    }
    return table;
}

// https://rosettacode.org/wiki/CRC-32#C
// https://github.com/Dillonb/n64/blob/e015f9dddf82d4d8c813ff3a16d7044965acde86/src/mem/n64rom.c#L60C1-L60C53
uint32_t crc32(uint32_t crc, const uint8_t *buffer, const size_t length) {
    constexpr std::array<uint32_t, CRC32_TABLE_SIZE> table = crc32_table();

    crc = ~crc;
    const uint8_t *p_end = buffer + length;
    for (const uint8_t *p = buffer; p < p_end; p++) {
        const uint8_t octet = *p;
        crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
    }

    return ~crc;
}

CicType checksum_to_cic(uint32_t checksum) {
    using c = CicType;

    const auto result = checksum == 0xEC8B1325   ? c::CIC_NUS_7102
                        : checksum == 0x1DEB51A9 ? c::CIC_NUS_6101
                        : checksum == 0xC08E5BD6 ? c::CIC_NUS_6102_7101
                        : checksum == 0x03B8376A ? c::CIC_NUS_6103_7103
                        : checksum == 0xCF7F41DC ? c::CIC_NUS_6105_7105
                        : checksum == 0xD1059C6A ? c::CIC_NUS_6106_7106
                                                 : c::CIC_UNKNOWN;

    if (result == c::CIC_UNKNOWN) {
        spdlog::error("invalid checksum: {:#08x}", checksum);
    }

    return result;
}

void Rom::read_from(const std::string &filepath) {

    spdlog::debug("reading from ROM");

    std::ifstream file(filepath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Could not open ROM file");
        return;
    }
    rom = {std::istreambuf_iterator<char>(file),
           std::istreambuf_iterator<char>()};
    // size of phisical memory region mapped to memory
    rom.resize(0xFC00000);

    if (rom.size() < sizeof(rom_header_t)) {
        spdlog::error("ROM is too small");
        return;
    }

    // set header
    header = *reinterpret_cast<rom_header_t *>(rom.data());

    // set cic
    const uint32_t checksum = crc32(0, &rom[0x40], 0x9C0);
    cic = checksum_to_cic(checksum);

    spdlog::debug("ROM size\t= {}", rom.size());
    spdlog::debug("imageName\t= \"{}\"", std::string(header.image_name));
    // spdlog::debug("CIC\t= {}", cic); // TODO: enumをstringにできるように

    broken = false;
}

} // namespace Memory
} // namespace N64
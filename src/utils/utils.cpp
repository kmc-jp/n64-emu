#include "cpu/cpu.h"
#include <cassert>
#include <cstdint>
#include <source_location>

namespace Utils {

void write_to_byte_array64(std::span<uint8_t> span, uint64_t offset,
                           uint64_t value) {
    assert(offset + 8 <= span.size());
    uint8_t b1 = value & 0xFF;
    uint8_t b2 = (value >> 8) & 0xff;
    uint8_t b3 = (value >> 16) & 0xff;
    uint8_t b4 = (value >> 24) & 0xff;
    uint8_t b5 = (value >> 32) & 0xFF;
    uint8_t b6 = (value >> 40) & 0xff;
    uint8_t b7 = (value >> 48) & 0xff;
    uint8_t b8 = (value >> 56) & 0xff;

    span[offset + 0] = b8;
    span[offset + 1] = b7;
    span[offset + 2] = b6;
    span[offset + 3] = b5;
    span[offset + 4] = b4;
    span[offset + 5] = b3;
    span[offset + 6] = b2;
    span[offset + 7] = b1;
}

void write_to_byte_array32(std::span<uint8_t> span, uint64_t offset,
                           uint32_t value) {
    assert(offset + 4 <= span.size());
    uint8_t b1 = value & 0xFF;
    uint8_t b2 = (value >> 8) & 0xff;
    uint8_t b3 = (value >> 16) & 0xff;
    uint8_t b4 = (value >> 24) & 0xff;
    span[offset + 0] = b4;
    span[offset + 1] = b3;
    span[offset + 2] = b2;
    span[offset + 3] = b1;
}

void write_to_byte_array16(std::span<uint8_t> span, uint64_t offset,
                           uint16_t value) {
    assert(offset + 2 <= span.size());
    uint8_t b1 = value & 0xFF;
    uint8_t b2 = (value >> 8) & 0xff;
    span[offset + 0] = b2;
    span[offset + 1] = b1;
}

void write_to_byte_array8(std::span<uint8_t> span, uint64_t offset,
                          uint8_t value) {
    assert(offset + 1 <= span.size());
    span[offset] = value;
}

std::vector<uint8_t> read_binary_file(std::string filepath) {
    std::ifstream file(filepath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        Utils::abort("Could not open binary file: {}", filepath);
        exit(-1);
    }
    // determine file size
    uint64_t file_size = file.tellg();
    // go to the last byte
    file.seekg(0, std::ios::end);
    file_size = static_cast<uint64_t>(file.tellg()) - file_size;
    // go back to the first byte
    file.clear();
    file.seekg(0);

    std::vector<uint8_t> ret;
    ret.assign(file_size, 0);

    // Read entire ROM data
    file.read(reinterpret_cast<char *>(ret.data()), file_size);

    return ret;
}

} // namespace Utils

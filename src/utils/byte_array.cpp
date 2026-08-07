#include "utils/byte_array.h"
#include <cassert>

namespace Utils {

uint64_t read_from_byte_array64(std::span<const uint8_t> span,
                                uint64_t offset) {
    assert(offset + 8 <= span.size());
    return (static_cast<uint64_t>(span[offset + 0]) << 56) +
           (static_cast<uint64_t>(span[offset + 1]) << 48) +
           (static_cast<uint64_t>(span[offset + 2]) << 40) +
           (static_cast<uint64_t>(span[offset + 3]) << 32) +
           (static_cast<uint64_t>(span[offset + 4]) << 24) +
           (static_cast<uint64_t>(span[offset + 5]) << 16) +
           (static_cast<uint64_t>(span[offset + 6]) << 8) +
           (static_cast<uint64_t>(span[offset + 7]) << 0);
}

uint32_t read_from_byte_array32(std::span<const uint8_t> span,
                                uint64_t offset) {
    assert(offset + 4 <= span.size());
    return (span[offset + 0] << 24) + (span[offset + 1] << 16) +
           (span[offset + 2] << 8) + (span[offset + 3] << 0);
}

uint16_t read_from_byte_array16(std::span<const uint8_t> span,
                                uint64_t offset) {
    assert(offset + 2 <= span.size());
    return (static_cast<uint16_t>(span[offset + 0] << 8)) +
           (static_cast<uint16_t>(span[offset + 1] << 0));
}

uint8_t read_from_byte_array8(std::span<const uint8_t> span, uint64_t offset) {
    assert(offset + 1 <= span.size());
    return span[offset];
}

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

} // namespace Utils

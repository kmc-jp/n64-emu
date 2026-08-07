#include "utils/byte_array.h"
#include <cassert>
#include <cstring>

namespace Utils {

uint64_t read_from_byte_array64(std::span<const uint8_t> span,
                                uint64_t offset) {
    assert(offset + 8 <= span.size());
    // Match Dillonb: high word first in memory, each word host-endian.
    uint32_t hi = 0;
    uint32_t lo = 0;
    std::memcpy(&hi, span.data() + offset, sizeof(uint32_t));
    std::memcpy(&lo, span.data() + offset + sizeof(uint32_t), sizeof(uint32_t));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

uint32_t read_from_byte_array32(std::span<const uint8_t> span,
                                uint64_t offset) {
    assert(offset + 4 <= span.size());
    uint32_t val = 0;
    std::memcpy(&val, span.data() + offset, sizeof(uint32_t));
    return val;
}

uint16_t read_from_byte_array16(std::span<const uint8_t> span,
                                uint64_t offset) {
    const uint64_t idx = half_address(static_cast<uint32_t>(offset));
    assert(idx + 2 <= span.size());
    uint16_t val = 0;
    std::memcpy(&val, span.data() + idx, sizeof(uint16_t));
    return val;
}

uint8_t read_from_byte_array8(std::span<const uint8_t> span, uint64_t offset) {
    const uint64_t idx = byte_address(static_cast<uint32_t>(offset));
    assert(idx + 1 <= span.size());
    return span[idx];
}

uint32_t read_from_byte_array32_be(std::span<const uint8_t> span,
                                   uint64_t offset) {
    assert(offset + 4 <= span.size());
    return (static_cast<uint32_t>(span[offset + 0]) << 24) |
           (static_cast<uint32_t>(span[offset + 1]) << 16) |
           (static_cast<uint32_t>(span[offset + 2]) << 8) |
           static_cast<uint32_t>(span[offset + 3]);
}

uint16_t read_from_byte_array16_be(std::span<const uint8_t> span,
                                   uint64_t offset) {
    assert(offset + 2 <= span.size());
    return static_cast<uint16_t>((span[offset + 0] << 8) | span[offset + 1]);
}

void write_to_byte_array64(std::span<uint8_t> span, uint64_t offset,
                           uint64_t value) {
    assert(offset + 8 <= span.size());
    const uint32_t hi = static_cast<uint32_t>(value >> 32);
    const uint32_t lo = static_cast<uint32_t>(value);
    std::memcpy(span.data() + offset, &hi, sizeof(uint32_t));
    std::memcpy(span.data() + offset + sizeof(uint32_t), &lo, sizeof(uint32_t));
}

void write_to_byte_array32(std::span<uint8_t> span, uint64_t offset,
                           uint32_t value) {
    assert(offset + 4 <= span.size());
    std::memcpy(span.data() + offset, &value, sizeof(uint32_t));
}

void write_to_byte_array16(std::span<uint8_t> span, uint64_t offset,
                           uint16_t value) {
    const uint64_t idx = half_address(static_cast<uint32_t>(offset));
    assert(idx + 2 <= span.size());
    std::memcpy(span.data() + idx, &value, sizeof(uint16_t));
}

void write_to_byte_array8(std::span<uint8_t> span, uint64_t offset,
                          uint8_t value) {
    const uint64_t idx = byte_address(static_cast<uint32_t>(offset));
    assert(idx + 1 <= span.size());
    span[idx] = value;
}

void write_to_byte_array32_be(std::span<uint8_t> span, uint64_t offset,
                              uint32_t value) {
    assert(offset + 4 <= span.size());
    span[offset + 0] = static_cast<uint8_t>(value >> 24);
    span[offset + 1] = static_cast<uint8_t>(value >> 16);
    span[offset + 2] = static_cast<uint8_t>(value >> 8);
    span[offset + 3] = static_cast<uint8_t>(value);
}

void write_to_byte_array16_be(std::span<uint8_t> span, uint64_t offset,
                              uint16_t value) {
    assert(offset + 2 <= span.size());
    span[offset + 0] = static_cast<uint8_t>(value >> 8);
    span[offset + 1] = static_cast<uint8_t>(value);
}

void write_to_byte_array64_be(std::span<uint8_t> span, uint64_t offset,
                              uint64_t value) {
    assert(offset + 8 <= span.size());
    write_to_byte_array32_be(span, offset,
                             static_cast<uint32_t>(value >> 32));
    write_to_byte_array32_be(span, offset + 4, static_cast<uint32_t>(value));
}

void byteswap_to_host(std::span<uint8_t> data) {
    // Convert big-endian word bytes (z64) to host-endian word storage.
    for (size_t i = 0; i + 4 <= data.size(); i += 4) {
        uint32_t w = 0;
        std::memcpy(&w, data.data() + i, sizeof(uint32_t));
        w = ((w & 0x000000FFu) << 24) | ((w & 0x0000FF00u) << 8) |
            ((w & 0x00FF0000u) >> 8) | ((w & 0xFF000000u) >> 24);
        std::memcpy(data.data() + i, &w, sizeof(uint32_t));
    }
}

} // namespace Utils

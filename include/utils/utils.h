#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Utils {

template <class...> constexpr std::false_type always_false{};

/* 指定された配列から8byte分を読み込む (big endian) */
inline uint64_t read_from_byte_array64(std::span<const uint8_t> span,
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

/* 指定された配列から4byte分を読み込む (big endian) */
inline uint32_t read_from_byte_array32(std::span<const uint8_t> span,
                                       uint64_t offset) {
    assert(offset + 4 <= span.size());
    return (span[offset + 0] << 24) + (span[offset + 1] << 16) +
           (span[offset + 2] << 8) + (span[offset + 3] << 0);
}

/* 指定された配列から2byte分を読み込む (big endian) */
inline uint16_t read_from_byte_array16(std::span<const uint8_t> span,
                                       uint64_t offset) {
    assert(offset + 2 <= span.size());
    return (static_cast<uint16_t>(span[offset + 0] << 8)) +
           (static_cast<uint16_t>(span[offset + 1] << 0));
}

/* 指定された配列から1byte分を読み込む (big endian) */
inline uint8_t read_from_byte_array8(std::span<const uint8_t> span,
                                     uint64_t offset) {
    assert(offset + 1 <= span.size());
    return span[offset];
}

/* 指定された配列からWire分を読み込む (big endian) */
template <typename Wire>
inline Wire read_from_byte_array(std::span<const uint8_t> span,
                                 uint64_t offset) {
    static_assert(std::is_same<Wire, uint8_t>::value ||
                  std::is_same<Wire, uint16_t>::value ||
                  std::is_same<Wire, uint32_t>::value ||
                  std::is_same<Wire, uint64_t>::value);

    if constexpr (std::is_same<Wire, uint8_t>::value) {
        return read_from_byte_array8(span, offset);
    } else if constexpr (std::is_same<Wire, uint16_t>::value) {
        return read_from_byte_array16(span, offset);
    } else if constexpr (std::is_same<Wire, uint32_t>::value) {
        return read_from_byte_array32(span, offset);
    } else if constexpr (std::is_same<Wire, uint64_t>::value) {
        return read_from_byte_array64(span, offset);
    } else {
        static_assert(always_false<Wire>);
    }
}

/* 指定された配列に8byte分を書き込む (big endian) */
void write_to_byte_array64(std::span<uint8_t> span, uint64_t offset,
                           uint64_t value);

/* 指定された配列に4byte分を書き込む (big endian) */
void write_to_byte_array32(std::span<uint8_t> span, uint64_t offset,
                           uint32_t value);

/* 指定された配列に2byte分を書き込む (big endian) */
void write_to_byte_array16(std::span<uint8_t> span, uint64_t offset,
                           uint16_t value);

/* 指定された配列に1byte分を書き込む (big endian) */
void write_to_byte_array8(std::span<uint8_t> span, uint64_t offset,
                          uint8_t value);

std::vector<uint8_t> read_binary_file(std::string filepath);

} // namespace Utils

#endif

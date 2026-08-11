#ifndef INCLUDE_GUARD_389EA714_261C_4B70_B700_783D518A6B35
#define INCLUDE_GUARD_389EA714_261C_4B70_B700_783D518A6B35

#include <cstdint>
#include <span>
#include <type_traits>

namespace Utils {
template <class...> constexpr std::false_type always_false{};

// Host-endian RDRAM/ROM/IMEM layout expected by paraLLEl-RDP.
// On little-endian hosts, bytes/halfwords are addressed with XOR within each word.
inline constexpr uint32_t byte_address(uint32_t addr) { return addr ^ 3u; }
inline constexpr uint32_t half_address(uint32_t addr) { return addr ^ 2u; }

/* Read 8 bytes from the array (host endian). */
uint64_t read_from_byte_array64(std::span<const uint8_t> span, uint64_t offset);

/* Read 4 bytes from the array (host endian). */
uint32_t read_from_byte_array32(std::span<const uint8_t> span, uint64_t offset);

/* Read 2 bytes from the array (host endian, addr^2). */
uint16_t read_from_byte_array16(std::span<const uint8_t> span, uint64_t offset);

/* Read 1 byte from the array (host endian, addr^3). */
uint8_t read_from_byte_array8(std::span<const uint8_t> span, uint64_t offset);

/* Big-endian byte-array accessors (SP DMEM layout) */
uint32_t read_from_byte_array32_be(std::span<const uint8_t> span,
                                   uint64_t offset);
uint16_t read_from_byte_array16_be(std::span<const uint8_t> span,
                                   uint64_t offset);
void write_to_byte_array32_be(std::span<uint8_t> span, uint64_t offset,
                              uint32_t value);
void write_to_byte_array16_be(std::span<uint8_t> span, uint64_t offset,
                              uint16_t value);
void write_to_byte_array64_be(std::span<uint8_t> span, uint64_t offset,
                              uint64_t value);

/* Read Wire bytes from the array (host endian). */
template <typename Wire>
Wire read_from_byte_array(std::span<const uint8_t> span, uint64_t offset) {
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

/* Write 8 bytes to the array (host endian). */
void write_to_byte_array64(std::span<uint8_t> span, uint64_t offset,
                           uint64_t value);

/* Write 4 bytes to the array (host endian). */
void write_to_byte_array32(std::span<uint8_t> span, uint64_t offset,
                           uint32_t value);

/* Write 2 bytes to the array (host endian, addr^2). */
void write_to_byte_array16(std::span<uint8_t> span, uint64_t offset,
                           uint16_t value);

/* Write 1 byte to the array (host endian, addr^3). */
void write_to_byte_array8(std::span<uint8_t> span, uint64_t offset,
                          uint8_t value);

/* Swap big-endian words in-place to host endian (for ROM after CIC detect). */
void byteswap_to_host(std::span<uint8_t> data);
} // namespace Utils

#endif

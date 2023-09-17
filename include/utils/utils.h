#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#ifdef __SIZEOF_INT128__ // GNU C
using int128_t = __int128;
using uint128_t = unsigned __int128;
#endif

// This macro defines mul_unsigned_hi, which returns uppper 64bit of u64 * u64
// This code is retrieved from
// https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
#ifdef __SIZEOF_INT128__ // GNU C
static inline uint64_t mul_unsigned_hi(uint64_t a, uint64_t b) {
    uint128_t prod = a * (uint128_t)b;
    return prod >> 64;
}
#elif defined(_M_X64) || defined(_M_ARM64) // MSVC
// MSVC for x86-64 or AArch64
// possibly also  || defined(_M_IA64) || defined(_WIN64)
// but the docs only guarantee x86-64!  Don't use *just* _WIN64; it doesn't
// include AArch64 Android / Linux

// https://learn.microsoft.com/en-gb/cpp/intrinsics/umulh
#include <intrin.h>
#define mul_unsigned_hi __umulh
#elif defined(_M_IA64) // || defined(_M_ARM)       // MSVC again
// https://learn.microsoft.com/en-gb/cpp/intrinsics/umul128
// incorrectly say that _umul128 is available for ARM
// which would be weird because there's no single insn on AArch32
#include <intrin.h>
static inline uint64_t mul_unsigned_hi(uint64_t a, uint64_t b) {
    unsigned __int64 HighProduct;
    (void)_umul128(a, b, &HighProduct);
    return HighProduct;
}
#else
static_assert(false, "mul_unsigned_hi not implemented for this compiler");
#endif

// This macro defines mul_signed_hi, which returns uppper 64bit of i64 * i64
// TODO: Testing
#ifdef __SIZEOF_INT128__ // GNU C
// TODO: not tested
static inline uint64_t mul_signed_hi(int64_t a, int64_t b) {
    int128_t prod = a * (int128_t)b;
    return (uint128_t)prod >> 64;
}
#elif defined(_M_X64) || defined(_M_ARM64) // MSVC
// MSVC for x86-64 or AArch64
// possibly also  || defined(_M_IA64) || defined(_WIN64)
// but the docs only guarantee x86-64!  Don't use *just* _WIN64; it doesn't
// include AArch64 Android / Linux

// https://learn.microsoft.com/en-gb/cpp/intrinsics/umulh
#include <intrin.h>
#define mul_signed_hi __mulh
#elif defined(_M_IA64) // || defined(_M_ARM)       // MSVC again
// TODO: not tested
// https://learn.microsoft.com/en-gb/cpp/intrinsics/umul128
// incorrectly say that _umul128 is available for ARM
// which would be weird because there's no single insn on AArch32
#include <intrin.h>
static inline uint64_t mul_signed_hi(int64_t a, int64_t b) {
    __int64 HighProduct;
    (void)_mul128(a, b, &HighProduct);
    return (__uint64)HighProduct;
}
#else
static_assert(false, "mul_signed_hi not implemented for this compiler");
#endif

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

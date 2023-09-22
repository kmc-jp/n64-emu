#ifndef INCLUDE_GUARD_45AED875_6355_4E74_9280_5029CF598FC3
#define INCLUDE_GUARD_45AED875_6355_4E74_9280_5029CF598FC3

#include <cstdint>

#ifdef __SIZEOF_INT128__ // GNU C
using int128_t = __int128;
using uint128_t = unsigned __int128;
#endif

// This macro defines mul_unsigned_hi, which returns uppper 64bit of u64 * u64
// This code is retrieved from
// https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication
#if defined(__SIZEOF_INT128__) // GNU C
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
#error "mul_unsigned_hi not implemented for this compiler"
#endif

// This macro defines mul_signed_hi, which returns uppper 64bit of i64 * i64
// TODO: Testing
#if defined(__SIZEOF_INT128__) // GNU C
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
#error "mul_signed_hi not implemented for this compiler"
#endif

#endif

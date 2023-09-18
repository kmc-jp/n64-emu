#ifndef INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04
#define INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04

#include "utils/constant.h"
#include <concepts>
#include <type_traits>

namespace Utils {
// Controls read/write to a specific range of bits in a bit sequence pointed to
// by ptr
// T: constant_t type that represents the range of bits
// I: index_t type that represents the offset of the range
template <Constant T, Index I> struct ref_t {
    using value_type = typename T::value_type;

  private:
    static constexpr std::size_t offset = I::value;
    static constexpr value_type mask = T::value << offset;

  public:
    // ptr: pointer to the bit sequence
    value_type *ptr;
    ref_t(value_type &raw) : ptr(&raw) {}
    // Read the value from the range
    [[nodiscard]] value_type operator*() const {
        return (*ptr & mask) >> offset;
    }
    // Write the value to the range
    void operator=(value_type v) {
        v <<= offset;
        *ptr = (*ptr & ~mask) | (v & mask);
    }
};

// Base class to help creating bitfield structures
// Publicly inherit this class and define members with field_t type
// ```
// struct example_t : bitfield_t<std::uint8_t> {
//   example(std::uint8_t &raw) : a{raw}, b{raw}, c{raw} {}
//   field_t<0, 2> a; // 2 bits at offset 0
//   field_t<2, 3> b; // 3 bits at offset 2
//   field_t<6, 1> c; // 1 bits at offset 6
// };
// std::uint8_t raw = 0;
// example_t ex{raw};
// ex.a = 0b11;  // raw == 00000011
// ex.b = 0b110; // raw == 00011011
// ex.c = 0b1;   // raw == 01011011
// raw = 0b01010101;
// assert(*ex.a == 0b01);
// assert(*ex.b == 0b101);
// assert(*ex.c == 0b1);
// ```
// T: unsigned integral type that represents the size of the bit sequence
template <std::unsigned_integral T> struct bitfield_t {
  private:
    static constexpr auto zero = constant_t<T, 0>{};
    static constexpr auto size = index_t<(sizeof(T) * 8)>{};
    template <std::size_t L>
    using range_t = decltype((~zero) >> (size - index_t<L>{}));

  public:
    // Type that represents a reference to a specific range of bits
    // I: the offset of the range
    // L: the length of the range
    template <std::size_t I, std::size_t L>
    using field_t = ref_t<range_t<L>, index_t<I>>;
};

} // namespace Utils

#endif // INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04

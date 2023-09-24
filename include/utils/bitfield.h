#ifndef INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04
#define INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04

#include "utils/constant.h"
#include <concepts>
#include <spdlog/fmt/bundled/core.h>
#include <type_traits>

namespace Utils {
/// @brief Represents bits with a specific width and offset
/// @tparam T underlying unsigned integral type of the bits
/// @tparam W width of the bits
/// @tparam I offset of the bits
template <std::unsigned_integral T, std::size_t W, std::size_t I = 0>
class Bits {
  private:
    template <std::unsigned_integral, std::size_t, std::size_t>
    friend class Bits;
    T value;
    static constexpr auto zero = IntegralConstant<T, 0>{};
    static constexpr auto size = IndexConstant<(sizeof(T) * 8)>{};
    static constexpr auto width = IndexConstant<W>{};
    static constexpr auto offset = IndexConstant<I>{};
    static_assert(0 < width && (width + offset) <= size);
    static constexpr auto mask = (~zero >> (size - width)) << offset;
    template <std::convertible_to<T> U> static constexpr T cast(U v) {
        return static_cast<T>(v);
    }
    template <std::size_t I2> constexpr auto shift() const {
        using B = Bits<T, W, I2>;
        constexpr auto offset2 = IndexConstant<I2>{};
        if constexpr (offset < offset2) {
            return B{std::in_place, value << (offset2 - offset)};
        } else {
            return B{std::in_place, value >> (offset - offset2)};
        }
    }
    template <std::convertible_to<T> V>
    constexpr Bits(std::in_place_t, V v) : value{cast(v)} {}
    constexpr T get(std::in_place_t) const { return cast(value & mask); }
    constexpr void set(Bits b) {
        value = cast(cast(value & ~mask) | b.get(std::in_place));
    }

  public:
    constexpr Bits() : Bits{std::in_place, 0} {}
    template <std::convertible_to<T> V>
    constexpr explicit Bits(V v) : Bits{Bits<T, W>{std::in_place, v}} {}
    template <std::unsigned_integral U, std::size_t I2>
    constexpr explicit Bits(Bits<U, W, I2> b) : Bits{b.template shift<I>()} {}
    template <std::unsigned_integral U>
    constexpr explicit Bits(Bits<U, W, I> b) : Bits{std::in_place, b.value} {}
    [[nodiscard]] constexpr operator T() const { return get(); }
    template <typename V> void operator=(V v) { set(Bits{v}); }
    template <typename U, std::size_t I2> void operator=(Bits<U, W, I2> b) {
        set(Bits{b});
    }
    template <typename V> constexpr V as() const {
        return static_cast<V>(get());
    }
    constexpr T get() const { return shift<0>().get(std::in_place); }
};

// Controls read/write to a specific range of bits in a bit sequence pointed to
// by ptr
// T: constant_t type that represents the range of bits
// I: index_t type that represents the offset of the range
template <Constant T, Index I> struct Ref {
    using value_type = typename T::value_type;

  private:
    static constexpr std::size_t offset = I::value;
    static constexpr value_type mask = T::value << offset;

  public:
    // ptr: pointer to the bit sequence
    value_type *ptr;
    Ref(value_type &raw) : ptr(&raw) {}
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
// struct Example : Bitfield<std::uint8_t> {
//   Example(std::uint8_t &raw) : a{raw}, b{raw}, c{raw} {}
//   Field<0, 1> a; // 2 bits at offset 0
//   Field<2, 4> b; // 3 bits at offset 2
//   Field<6, 6> c; // 1 bits at offset 6
// };
// std::uint8_t raw = 0;
// Example ex{raw};
// ex.a = 0b11;  // raw == 00000011
// ex.b = 0b110; // raw == 00011011
// ex.c = 0b1;   // raw == 01011011
// raw = 0b01010101;
// assert(*ex.a == 0b01);
// assert(*ex.b == 0b101);
// assert(*ex.c == 0b1);
// ```
// T: unsigned integral type that represents the size of the bit sequence
template <std::unsigned_integral T> struct Bitfield {
  private:
    static constexpr auto zero = IntegralConstant<T, 0>{};
    static constexpr auto size = IndexConstant<(sizeof(T) * 8)>{};
    template <std::size_t L>
    using Range = decltype((~zero) >> (size - IndexConstant<L>{}));

  public:
    // Type that represents a reference to a specific range of bits
    // B: the beginning of the range
    // E: the end of the range, inclusive
    template <std::size_t B, std::size_t E,
              std::enable_if_t<(B <= E), std::nullptr_t> = nullptr>
    using Field = Ref<Range<(E - B + 1)>, IndexConstant<B>>;
};

} // namespace Utils

template <std::unsigned_integral T, std::size_t W, std::size_t I>
struct fmt::formatter<Utils::Bits<T, W, I>> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }
    auto format(const Utils::Bits<T, W, I> &b, format_context &ctx) const {
        return fmt::format_to(ctx.out(), "{:#b}", b.get());
    }
};

#endif // INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04

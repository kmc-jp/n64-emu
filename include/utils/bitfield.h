#ifndef INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04
#define INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04

#include "utils/constant.h"
#include <concepts>
#include <spdlog/fmt/bundled/core.h>
#include <type_traits>

namespace Utils {
template <std::unsigned_integral, std::size_t, std::size_t> struct BitsRef;

/// @brief Represents bits with a specific width and offset
/// @tparam T underlying unsigned integral type of the bits
/// @tparam W width of the bits
/// @tparam I offset of the bits
template <std::unsigned_integral T, std::size_t W, std::size_t I = 0>
class Bits {
  private:
    template <std::unsigned_integral, std::size_t, std::size_t>
    friend class Bits;
    template <std::unsigned_integral, std::size_t, std::size_t>
    friend class BitsRef;
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

/// @brief Controls read/write to a specific range of bits in a bit sequence
/// @tparam T underlying unsigned integral type of the bit sequence
/// @tparam W width of the bit sequence
/// @tparam I offset of the bit sequence
template <std::unsigned_integral T, std::size_t W, std::size_t I>
class BitsRef {
  private:
    using B = Bits<T, W, I>;
    T &ref;

  public:
    /// @param raw reference to the bit sequence
    BitsRef(T &raw) : ref(raw) {}
    /// @brief Read the value from bits in the range
    [[nodiscard]] auto operator()() const { return B{std::in_place, ref}; }
    /// @brief Write the value to bits in the range
    template <typename V> void operator=(V v) {
        B b{std::in_place, ref};
        b = v;
        ref = b.value;
    }
};

/// @brief Base class to help creating bitfield structures
/// @details Publicly inherit this class and define members with Field type
/// @code {.cpp}
/// struct Example : Bitfield<std::uint8_t> {
///   Example(std::uint8_t &raw) : a{raw}, b{raw}, c{raw} {}
///   Field<0, 1> a; // Bits: 0, 1
///   Field<2, 4> b; // Bits: 2, 3, 4
///   Field<6> c;  // Bits: 6
/// };
/// std::uint8_t raw = 0;
/// Example ex{raw};
/// ex.a = 0b11;  // raw == 0b00000011
/// ex.b = 0b110; // raw == 0b00011011
/// ex.c = 0b1;   // raw == 0b01011011
/// raw = 0b01010101;
/// assert(0b01 == ex.a);
/// assert(0b101 == ex.b);
/// assert(0b1 == ex.c);
/// @endcode
///
/// @tparam T underlying unsigned integral type of the bit sequence
template <std::unsigned_integral T> struct Bitfield {
    /// @brief Field type to control read/write to a specific range of bits
    /// @tparam B beginning index of the range
    /// @tparam E ending index of the range
    template <std::size_t B, std::size_t E = B>
    using Field = BitsRef<T, (E - B + 1), B>;
};
} // namespace Utils

template <std::unsigned_integral T, std::size_t W, std::size_t I>
struct fmt::formatter<Utils::Bits<T, W, I>> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }
    auto format(const Utils::Bits<T, W, I> &b, format_context &ctx) const {
        return fmt::format_to(ctx.out(), "{:#b}", b.get());
    }
};
template <std::unsigned_integral T, std::size_t W, std::size_t I>
struct fmt::formatter<typename Utils::BitsRef<T, W, I>> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }
    auto format(const typename Utils::BitsRef<T, W, I> &r,
                format_context &ctx) const {
        return fmt::format_to(ctx.out(), "{:#b}", r().get());
    }
};

#endif // INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04

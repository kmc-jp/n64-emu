#ifndef INCLUDE_GUARD_1FE5F0F6_A0D1_47B1_9BD1_4BCF83BAC874
#define INCLUDE_GUARD_1FE5F0F6_A0D1_47B1_9BD1_4BCF83BAC874

#include "utils/constant.h"
#include <concepts>
#include <cstddef>
#include <spdlog/fmt/bundled/core.h>
#include <utility>

namespace Utils {
template <UnsignedIntegral, std::size_t, std::size_t> struct BitsRef;

/// @brief Represents bits with a specific width and offset
/// @tparam T underlying unsigned integral type of the bits
/// @tparam W width of the bits
/// @tparam I offset of the bits
template <UnsignedIntegral T, std::size_t W, std::size_t I = 0> class Bits {
  private:
    template <UnsignedIntegral, std::size_t, std::size_t> friend class Bits;
    template <UnsignedIntegral, std::size_t, std::size_t> friend class BitsRef;
    T value;
    template <std::convertible_to<T> U> static constexpr T cast(U v) {
        return static_cast<T>(v);
    }
    template <std::size_t I2> constexpr auto shift() const {
        using B = Bits<T, W, I2>;
        if constexpr (I < I2) {
            return B{std::in_place, value << (I2 - I)};
        } else {
            return B{std::in_place, value >> (I - I2)};
        }
    }
    template <std::convertible_to<T> V>
    constexpr Bits(std::in_place_t, V v) : value{cast(v)} {}
    static constexpr auto mask = Utils::mask_const<T, W, I>;
    constexpr T get(std::in_place_t) const { return cast(value & mask); }
    constexpr void set(Bits b) {
        value = cast(cast(value & ~mask) | b.get(std::in_place));
    }

  public:
    constexpr Bits() : Bits{std::in_place, 0} {}
    template <std::convertible_to<T> V>
    constexpr explicit Bits(V v) : Bits{Bits<T, W>{std::in_place, v}} {}
    template <UnsignedIntegral U, std::size_t I2>
    constexpr explicit Bits(Bits<U, W, I2> b) : Bits{b.template shift<I>()} {}
    template <UnsignedIntegral U>
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
} // namespace Utils

template <Utils::UnsignedIntegral T, std::size_t W, std::size_t I>
struct fmt::formatter<Utils::Bits<T, W, I>> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }
    auto format(const Utils::Bits<T, W, I> &b, format_context &ctx) const {
        return fmt::format_to(ctx.out(), "{:#b}", b.get());
    }
};

#endif // INCLUDE_GUARD_1FE5F0F6_A0D1_47B1_9BD1_4BCF83BAC874

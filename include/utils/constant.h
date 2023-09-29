#ifndef INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F
#define INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace Utils {
template <std::unsigned_integral T, T V> struct IntegralConstant;
template <typename T> struct IsConstant : std::false_type {};
template <std::unsigned_integral T, T V>
struct IsConstant<IntegralConstant<T, V>> : std::true_type {};

template <typename T>
concept Constant = IsConstant<T>::value;
template <typename T>
concept Index =
    Constant<T> && std::is_same_v<typename T::value_type, std::size_t>;

template <std::unsigned_integral T, T V>
struct IntegralConstant : std::integral_constant<T, V> {
    using typename std::integral_constant<T, V>::value_type;

  private:
    template <value_type U> using Self = IntegralConstant<value_type, U>;
    template <typename U> static consteval value_type cast(U v) {
        return static_cast<value_type>(v);
    }
    static constexpr value_type lhs = V;

  public:
    template <value_type W>
    consteval auto operator+(Self<W> rhs) const -> decltype(auto) {
        return Self<cast(lhs + rhs)>{};
    }
    template <value_type W>
    consteval auto operator-(Self<W> rhs) const -> decltype(auto) {
        return Self<cast(lhs - rhs)>{};
    }
    template <value_type W>
    consteval auto operator&(Self<W> rhs) const -> decltype(auto) {
        return Self<cast(lhs & rhs)>{};
    }
    template <value_type W>
    consteval auto operator|(Self<W> rhs) const -> decltype(auto) {
        return Self<cast(lhs | rhs)>{};
    }
    template <value_type W>
    consteval auto operator^(Self<W> rhs) const -> decltype(auto) {
        return Self<cast(lhs ^ rhs)>{};
    }
    consteval auto operator~() const -> decltype(auto) {
        return Self<cast(~lhs)>{};
    }
    template <Index I>
    consteval auto operator<<(I rhs) const -> decltype(auto) {
        return Self<cast(lhs << rhs)>{};
    }
    template <Index I>
    consteval auto operator>>(I rhs) const -> decltype(auto) {
        return Self<cast(lhs >> rhs)>{};
    }
    template <value_type W>
    consteval auto min(Self<W> rhs) const -> decltype(auto) {
        return Self<((lhs < rhs) ? lhs : rhs)>{};
    }
    template <value_type W>
    consteval auto max(Self<W> rhs) const -> decltype(auto) {
        return Self<((lhs < rhs) ? rhs : lhs)>{};
    }
};

template <std::size_t I>
constexpr auto index_const = IntegralConstant<std::size_t, I>{};
template <std::unsigned_integral T>
constexpr auto zero_const = IntegralConstant<T, 0>{};
template <std::unsigned_integral T, std::size_t W = sizeof(T) * 8,
          std::size_t I = 0,
          std::enable_if_t<(W + I <= sizeof(T) * 8), std::nullptr_t> = nullptr>
constexpr auto mask_const =
    ((~zero_const<T>) >> (index_const<sizeof(T) * 8 - W>)) << (index_const<I>);
} // namespace Utils

#endif // INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F

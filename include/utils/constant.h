#ifndef INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F
#define INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F

#include <concepts>
#include <type_traits>

namespace Utils {
template <std::unsigned_integral T, T V> struct constant_t;
template <typename T> struct is_constant : std::false_type {};
template <std::unsigned_integral T, T V>
struct is_constant<constant_t<T, V>> : std::true_type {};

template <typename T>
concept Constant = is_constant<T>::value;
template <typename T>
concept Index =
    Constant<T> && std::is_same_v<typename T::value_type, std::size_t>;

template <std::unsigned_integral T, T V>
struct constant_t : std::integral_constant<T, V> {
    using typename std::integral_constant<T, V>::value_type;

  private:
    template <value_type U> using self = constant_t<value_type, U>;
    template <typename U> static consteval value_type cast(U v) {
        return static_cast<value_type>(v);
    }
    static constexpr value_type lhs = V;

  public:
    template <value_type W>
    consteval auto operator+(self<W> rhs) const -> decltype(auto) {
        return self<cast(lhs + rhs)>{};
    }
    template <value_type W>
    consteval auto operator-(self<W> rhs) const -> decltype(auto) {
        return self<cast(lhs - rhs)>{};
    }
    template <value_type W>
    consteval auto operator&(self<W> rhs) const -> decltype(auto) {
        return self<cast(lhs & rhs)>{};
    }
    template <value_type W>
    consteval auto operator|(self<W> rhs) const -> decltype(auto) {
        return self<cast(lhs | rhs)>{};
    }
    template <value_type W>
    consteval auto operator^(self<W> rhs) const -> decltype(auto) {
        return self<cast(lhs ^ rhs)>{};
    }
    consteval auto operator~() const -> decltype(auto) {
        return self<cast(~lhs)>{};
    }
    template <Index I>
    consteval auto operator<<(I rhs) const -> decltype(auto) {
        return self<cast(lhs << rhs)>{};
    }
    template <Index I>
    consteval auto operator>>(I rhs) const -> decltype(auto) {
        return self<cast(lhs >> rhs)>{};
    }
    template <value_type W>
    consteval auto min(self<W> rhs) const -> decltype(auto) {
        return self<((lhs < rhs) ? lhs : rhs)>{};
    }
    template <value_type W>
    consteval auto max(self<W> rhs) const -> decltype(auto) {
        return self<((lhs < rhs) ? rhs : lhs)>{};
    }
};
template <std::size_t I> using index_t = constant_t<std::size_t, I>;

} // namespace Utils

#endif // INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F

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
concept Index = is_constant<T>::value &&
                std::is_same_v<typename T::value_type, std::size_t>;

template <std::unsigned_integral T, T V>
struct constant_t : std::integral_constant<T, V> {
    using typename std::integral_constant<T, V>::value_type;

  private:
    template <value_type U> using self = constant_t<value_type, U>;
    static constexpr std::size_t size = sizeof(value_type) * 8;
    static constexpr value_type zero = 0, one = 1;
    static constexpr value_type full = ~zero, half = one << (size - 1);

  public:
    template <value_type W>
    consteval auto operator+(self<W>) const -> decltype(auto) {
        if constexpr (V <= full - W) {
            return self<(V + W)>{};
        } else if constexpr (V >= half && W >= half) {
            return self<((V - half) + (W - half))>{};
        } else if constexpr (V < W) {
            return self<((V + (W - half)) - half)>{};
        } else {
            return self<(((V - half) + W) - half)>{};
        }
    }
    template <value_type W>
    consteval auto operator-(self<W>) const -> decltype(auto) {
        if constexpr (V < W) {
            return self<(V + (full - W) + one)>{};
        } else {
            return self<(V - W)>{};
        }
    }
    template <value_type W>
    consteval auto operator&(self<W>) const -> decltype(auto) {
        return self<(V & W)>{};
    }
    template <value_type W>
    consteval auto operator|(self<W>) const -> decltype(auto) {
        return self<(V | W)>{};
    }
    template <value_type W>
    consteval auto operator^(self<W>) const -> decltype(auto) {
        return self<(V ^ W)>{};
    }
    consteval auto operator~() const -> decltype(auto) {
        return self<static_cast<value_type>(~V)>{};
    }
    template <Index I> constexpr auto operator<<(I) const -> decltype(auto) {
        return self<((V & (full >> I::value)) << I::value)>{};
    }
    template <Index I> constexpr auto operator>>(I) const -> decltype(auto) {
        return self<(V >> I::value)>{};
    }
    template <value_type W>
    consteval auto min(self<W>) const -> decltype(auto) {
        return self<((V < W) ? V : W)>{};
    }
    template <value_type W>
    consteval auto max(self<W>) const -> decltype(auto) {
        return self<((V < W) ? W : V)>{};
    }
};
template <std::size_t I> using index_t = constant_t<std::size_t, I>;

} // namespace Utils

#endif // INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F

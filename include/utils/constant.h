#ifndef INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F
#define INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace Utils {
namespace detail {
template <typename T>
concept StdIntegralConstant = requires(T t) {
    // static constexpr V value = v;
    { T::value } -> std::convertible_to<typename T::value_type>;
    // using value_type = V;
    typename T::value_type;
    // using type       = integral_constant<V,v>;
    std::is_same_v<T, std::integral_constant<typename T::value_type, T::value>>;
    // constexpr operator value_type() noexcept;
    {
        t.operator typename T::value_type()
    } noexcept -> std::same_as<typename T::value_type>;
    // constexpr value_type operator()() const noexcept;
    { t.operator()() } noexcept -> std::same_as<typename T::value_type>;
};

template <typename T> struct IsUint128 : std::false_type {};
#ifdef __SIZEOF_INT128__
template <> struct IsUint128<unsigned __int128> : std::true_type {};
#endif
} // namespace detail

template <typename T>
concept Index = detail::StdIntegralConstant<T> &&
                std::is_same_v<std::size_t, typename T::value_type>;

template <typename T>
concept UnsignedIntegral =
    std::unsigned_integral<T> || detail::IsUint128<std::remove_cv_t<T>>::value;

template <UnsignedIntegral T, T V>
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

namespace detail {
template <typename> struct IsConstant : std::false_type {};
template <typename T, T V>
struct IsConstant<IntegralConstant<T, V>> : std::true_type {};
} // namespace detail
template <typename T>
concept Constant = detail::IsConstant<std::remove_cv_t<T>>::value;

template <std::size_t I>
constexpr auto index_const = IntegralConstant<std::size_t, I>{};
template <typename T> constexpr auto bits_of = index_const<sizeof(T) * 8>;

template <UnsignedIntegral T>
constexpr auto zero_const = IntegralConstant<T, 0>{};
template <UnsignedIntegral T, std::size_t W = bits_of<T>, std::size_t I = 0>
    requires(W + I <= bits_of<T>)
constexpr auto mask_const =
    ((~zero_const<T>) >> (index_const<bits_of<T> - W>)) << (index_const<I>);
} // namespace Utils

#endif // INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F

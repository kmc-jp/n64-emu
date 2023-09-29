#ifndef INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F
#define INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace Utils {
namespace detail {
template <std::size_t N> struct Uint;
template <> struct Uint<8> : std::type_identity<std::uint8_t> {};
template <> struct Uint<16> : std::type_identity<std::uint16_t> {};
template <> struct Uint<32> : std::type_identity<std::uint32_t> {};
template <> struct Uint<64> : std::type_identity<std::uint64_t> {};
#ifdef __SIZEOF_INT128__
#define UINT128_T_DEFINED
template <> struct Uint<128> : std::type_identity<unsigned __int128> {};
#endif
} // namespace detail
template <std::size_t N> using Uint = typename detail::Uint<N>::type;

template <typename T>
concept UnsignedIntegral =
    std::unsigned_integral<T> ||
    std::same_as<std::remove_cv_t<T>, Uint<(sizeof(T) * 8)>>;
template <UnsignedIntegral T>
using BitsOf = std::integral_constant<std::size_t, sizeof(T) * 8>;

template <UnsignedIntegral T, T V> struct IntegralConstant;
template <typename T> struct IsConstant : std::false_type {};
template <UnsignedIntegral T, T V>
struct IsConstant<IntegralConstant<T, V>> : std::true_type {};

template <typename T>
concept Constant = IsConstant<T>::value;
template <typename T>
concept Index =
    Constant<T> && std::is_same_v<typename T::value_type, std::size_t>;

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

template <std::size_t I>
constexpr auto index_const = IntegralConstant<std::size_t, I>{};
template <UnsignedIntegral T>
constexpr auto zero_const = IntegralConstant<T, 0>{};
template <UnsignedIntegral T, std::size_t W = BitsOf<T>{}, std::size_t I = 0,
          std::enable_if_t<(W + I <= BitsOf<T>{}), std::nullptr_t> = nullptr>
constexpr auto mask_const =
    ((~zero_const<T>) >> (index_const<BitsOf<T>{} - W>)) << (index_const<I>);
} // namespace Utils

#endif // INCLUDE_GUARD_21FEA685_23A5_43ED_A210_5762B13C987F

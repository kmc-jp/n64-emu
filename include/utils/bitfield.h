#ifndef INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04
#define INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04

#include "utils/constant.h"
#include <concepts>
#include <type_traits>

namespace Utils {
template <Constant T, Index I> struct ref_t {
    using value_type = typename T::value_type;

  private:
    static constexpr std::size_t offset = I::value;
    static constexpr value_type mask = T::value << offset;
    value_type unshift() const { return; }
    value_type shift(value_type v) const { return (v << offset) & mask; }

  public:
    value_type *ptr;
    ref_t(value_type &raw) : ptr(&raw) {}
    value_type operator*() const { return (*ptr & mask) >> offset; }
    void operator=(value_type v) {
        v <<= offset;
        *ptr = (*ptr & ~mask) | (v & mask);
    }
};

template <std::unsigned_integral T> struct bitfield_t {
  private:
    T *ptr;

  public:
    bitfield_t(T &raw) : ptr(&raw) {}
    template <std::size_t I, std::size_t L> auto field() -> decltype(auto) {
        constexpr auto zero = constant_t<T, 0>{};
        constexpr auto size = index_t<(sizeof(T) * 8)>{};
        constexpr auto length = index_t<L>{};
        using range_t = decltype((~zero) >> (size - size.min(length)));
        return ref_t<range_t, index_t<I>>{*ptr};
    }
};

} // namespace Utils

#endif // INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04

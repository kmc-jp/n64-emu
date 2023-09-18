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
    static constexpr auto zero = constant_t<T, 0>{};
    static constexpr auto size = index_t<(sizeof(T) * 8)>{};
    template <std::size_t L>
    using range_t = decltype((~zero) >> (size - index_t<L>{}));

  public:
    template <std::size_t I, std::size_t L>
    using field_t = ref_t<range_t<L>, index_t<I>>;
};

} // namespace Utils

#endif // INCLUDE_GUARD_B0CCED88_3605_4238_BB29_BCFF5395EF04

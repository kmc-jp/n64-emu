#ifndef INCLUDE_GUARD_2749A680_FCA0_46E8_9336_EBA6DB0A60F2
#define INCLUDE_GUARD_2749A680_FCA0_46E8_9336_EBA6DB0A60F2

#include <string_view>
#include <type_traits>

#if defined(__clang__) || defined(__GNUC__)
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define PRETTY_FUNCTION __FUNCSIG__
#else
#define PRETTY_FUNCTION __func__
#endif

namespace Utils {
class EnumView {
    using size_type = std::string_view::size_type;
    // TODO: add magic numbers for other compilers
#if defined(__clang__)
    static constexpr size_type prefix_offset = 38;
    static constexpr size_type suffix_offset = 1;
#elif defined(__GNUC__)
    static constexpr size_type prefix_offset = 58;
    static constexpr size_type suffix_offset = 1;
#elif defined(_MSC_VER)
    static constexpr size_type prefix_offset = 32;
    static constexpr size_type suffix_offset = 7;
#else
    static constexpr size_type prefix_offset = 0;
    static constexpr size_type suffix_offset = 0;
#endif
    template <auto E> static constexpr auto n() {
        constexpr std::string_view name{PRETTY_FUNCTION};
        return name.substr(prefix_offset,
                           name.size() - prefix_offset - suffix_offset);
    }
    template <bool B> using enabler_t = std::enable_if_t<B, std::nullptr_t>;

  public:
    template <auto E, enabler_t<std::is_enum_v<decltype(E)>> = nullptr>
    static constexpr auto value = n<E>();
};
} // namespace Utils

#endif

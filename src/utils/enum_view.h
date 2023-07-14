#ifndef INCLUDE_GUARD_2749A680_FCA0_46E8_9336_EBA6DB0A60F2
#define INCLUDE_GUARD_2749A680_FCA0_46E8_9336_EBA6DB0A60F2

#include <string_view>
#include <type_traits>

#if defined(__clang__)
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define PRETTY_FUNCTION __FUNCSIG__
#else
#define PRETTY_FUNCTION __func__
#endif

namespace Utils {
class EnumView {
    // TODO: add magic numbers for other compilers
#if defined(__clang__)
    static constexpr auto offset = 38;
#else
    static constexpr auto offset = 0;
#endif
    template <bool B> using enabler_t = std::enable_if_t<B, std::nullptr_t>;
    template <auto E, enabler_t<std::is_enum_v<decltype(E)>> = nullptr>
    static constexpr auto n() {
        constexpr std::string_view name{PRETTY_FUNCTION};
        return name.substr(offset, name.size() - offset - 1);
    }

  public:
    template <auto E> static constexpr auto value = n<E>();
};
} // namespace Utils

#endif

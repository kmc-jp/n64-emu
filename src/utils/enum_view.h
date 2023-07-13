#ifndef INCLUDE_GUARD_2749A680_FCA0_46E8_9336_EBA6DB0A60F2
#define INCLUDE_GUARD_2749A680_FCA0_46E8_9336_EBA6DB0A60F2

#include <string_view>
#include <type_traits>

namespace Utils {
class EnumView {
    // TODO: add magic numbers for other compilers
#if defined(__clang__)
    static constexpr auto offset = 38;
#else
    static constexpr auto offset = 0;
#endif
    template <bool B> using enabler_t = std::enable_if_t<B, std::nullptr_t>;
    template <typename T, enabler_t<std::is_enum_v<T>> = nullptr>
    static constexpr auto t() {
        return std::string_view{__PRETTY_FUNCTION__};
    }
    template <auto E, enabler_t<std::is_enum_v<decltype(E)>> = nullptr>
    static constexpr auto f() {
        return std::string_view{__PRETTY_FUNCTION__};
    }
    static constexpr std::string_view e(std::string_view name,
                                        std::string_view::size_type pos) {
        return name.substr(pos, name.size() - pos - 1);
    }

  public:
    template <auto E> static constexpr auto type = e(t<T>(), offset);
    template <auto E> static constexpr auto full = e(f<E>(), offset);
    template <auto E>
    static constexpr auto value = e(f<E>(), type<E>.size() + offset + 2);
};
} // namespace Utils

#endif

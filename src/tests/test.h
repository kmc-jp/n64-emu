#ifndef INCLUDE_GUARD_CEEB0D18_51A9_4EB2_B535_F45E29AFC936
#define INCLUDE_GUARD_CEEB0D18_51A9_4EB2_B535_F45E29AFC936

#include "utils/log.h"
#include <source_location>

namespace selftest {
[[noreturn]] void
test_fail(const std::source_location loc = std::source_location::current());

template <typename T, std::equality_comparable_with<T> U>
void test_eq(T expect, U actual,
             const std::source_location loc = std::source_location::current()) {
    if (expect != actual) {
        Utils::info("expect: {}, actual: {}", expect, actual);
        test_fail(loc);
    }
}

void bitfield_test();
} // namespace selftest

#endif // INCLUDE_GUARD_CEEB0D18_51A9_4EB2_B535_F45E29AFC936

// Unit testing
#include "test.h"
#include "utils/stdint.h"
#include <iostream>

namespace selftest {
[[noreturn]] void test_fail(const std::source_location loc) {
    std::cout << "Test failed at line " << loc.line() << " in "
              << loc.file_name() << std::endl;
    exit(-1);
}

void run_all() {
    mult_test();
    bitfield_test();
}
} // namespace selftest

int main() {
    selftest::run_all();
    return 0;
}

// Unit testing
#include "test.h"
#include "utils/stdint.h"
#include <ios>
#include <iostream>

namespace selftest {

[[noreturn]] void test_fail(const std::source_location loc) {
    std::cout << "Test failed at line " << loc.line() << " in "
              << loc.file_name() << std::endl;
    exit(-1);
}

void mult_test() {
    uint64_t uhi;

    uhi = mul_unsigned_hi(1, 1);
    if (uhi != 0)
        test_fail();

    uhi = mul_unsigned_hi(0x1234567890ABCDEF, 0xFEDCBA0987654321);
    if (uhi != 0x121fa000a3723a57)
        test_fail();

    uhi = (uint64_t)mul_signed_hi(1, -1);
    std::cout << std::hex << uhi << std::dec << std::endl;
    if (uhi != 0xffffffff'ffffffff)
        test_fail();

    uhi = (uint64_t)mul_signed_hi(0x0234567890ABCDEF, 0x0EDCBA0987654321);
    std::cout << std::hex << uhi << std::dec << std::endl;
    if (uhi != 0x20c34f035ad515)
        test_fail();
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

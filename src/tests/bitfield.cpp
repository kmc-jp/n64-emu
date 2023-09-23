#include "utils/bitfield.h"
#include "test.h"
#include <cstdint>

namespace Utils {
static_assert(Constant<IntegralConstant<unsigned short, 1>>);
static_assert(Constant<IntegralConstant<std::size_t, 2>>);
static_assert(Constant<IndexConstant<3>>);
static_assert(Index<IndexConstant<4>>);
static_assert(Index<IntegralConstant<std::size_t, 5>>);
} // namespace Utils
namespace {
void test1() {
    using selftest::test_eq;
    struct Bf : Utils::Bitfield<std::uint32_t> {
        Bf(std::uint32_t &raw)
            : a{raw}, b{raw}, c{raw}, d{raw}, e{raw}, lo{raw}, hi{raw} {}
        Field<0, 0> a;
        Field<1, 2> b;
        Field<3, 5> c;
        Field<6, 9> d;
        Field<10, 14> e;
        Field<0, 15> lo;
        Field<16, 31> hi;
    };
    std::uint32_t raw = 0, expect = 0;
    Bf bf{raw};
    test_eq(expect, raw);
    bf.a = 0b1;
    expect |= 0b1;
    test_eq(expect, raw);
    bf.b = 3;
    expect |= 0b110;
    test_eq(expect, raw);
    bf.c = 0b111;
    expect |= 0b111000;
    test_eq(expect, raw);
    bf.d = 0b1111;
    expect |= 0b1111000000;
    test_eq(expect, raw);
    bf.e = 0b11111;
    expect |= 0b111110000000000;
    test_eq(expect, raw);
    test_eq(0x7fff, raw);
    bf.hi = 0x4321;
    test_eq(0x4321'7fff, raw);
    bf.lo = 0x8765;
    test_eq(0x4321'8765, raw);
    test_eq(0b0100'0011'0010'0001'1'00001'1101'100'10'1, raw);
    test_eq(0b1, *bf.a);
    test_eq(0b10, *bf.b);
    test_eq(0b100, *bf.c);
    test_eq(0b1101, *bf.d);
    test_eq(0b00001, *bf.e);
    test_eq(0x8765, *bf.lo);
    test_eq(0x4321, *bf.hi);
}
void test2() {
    using selftest::test_eq;
    struct Example : Utils::Bitfield<std::uint8_t> {
        Example(std::uint8_t &raw) : a{raw}, b{raw}, c{raw} {}
        Field<0, 1> a; // 2 bits at offset 0
        Field<2, 4> b; // 3 bits at offset 2
        Field<6, 6> c; // 1 bits at offset 6
    };
    std::uint8_t raw = 0;
    Example ex{raw};
    ex.a = 0b11;
    test_eq(0b00000011, raw);
    ex.b = 0b110;
    test_eq(0b00011011, raw);
    ex.c = 0b1;
    test_eq(0b01011011, raw);
    raw = 0b01010101;
    test_eq(0b01, *ex.a);
    test_eq(0b101, *ex.b);
    test_eq(0b1, *ex.c);
}
} // namespace

namespace selftest {
void bitfield_test() {
    test1();
    test2();
}
} // namespace selftest

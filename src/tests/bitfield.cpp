#include "utils/bitfield.h"
#include "test.h"
#include <cstdint>

namespace Utils {
static_assert(Constant<constant_t<unsigned short, 1>>);
static_assert(Constant<constant_t<std::size_t, 2>>);
static_assert(Constant<index_t<3>>);
static_assert(Index<index_t<4>>);
static_assert(Index<constant_t<std::size_t, 5>>);
} // namespace Utils

namespace selftest {

struct bf_t : Utils::bitfield_t<std::uint32_t> {
    bf_t(std::uint32_t &raw) : bitfield_t(raw) {}
    auto a() -> decltype(auto) { return field<0, 1>(); }
    auto b() -> decltype(auto) { return field<1, 2>(); }
    auto c() -> decltype(auto) { return field<3, 3>(); }
    auto d() -> decltype(auto) { return field<6, 4>(); }
    auto e() -> decltype(auto) { return field<10, 5>(); }
    auto lo() -> decltype(auto) { return field<0, 16>(); }
    auto hi() -> decltype(auto) { return field<16, 16>(); }
};

void bitfield_test() {
    std::uint32_t raw = 0, expect = 0;
    bf_t bf{raw};
    test_eq(expect, raw);
    bf.a() = 0b1;
    expect |= 0b1;
    test_eq(expect, raw);
    bf.b() = 3;
    expect |= 0b110;
    test_eq(expect, raw);
    bf.c() = 0b111;
    expect |= 0b111000;
    test_eq(expect, raw);
    bf.d() = 0b1111;
    expect |= 0b1111000000;
    test_eq(expect, raw);
    bf.e() = 0b11111;
    expect |= 0b111110000000000;
    test_eq(expect, raw);
    test_eq(0x7fff, raw);
    bf.hi() = 0x4321;
    test_eq(0x4321'7fff, raw);
    bf.lo() = 0x8765;
    test_eq(0x4321'8765, raw);
    test_eq(0b0100'0011'0010'0001'1'00001'1101'100'10'1, raw);
    test_eq(0b1, *bf.a());
    test_eq(0b10, *bf.b());
    test_eq(0b100, *bf.c());
    test_eq(0b1101, *bf.d());
    test_eq(0b00001, *bf.e());
    test_eq(0x8765, *bf.lo());
    test_eq(0x4321, *bf.hi());
}

} // namespace selftest

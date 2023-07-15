#ifndef GPR_H
#define GPR_H

#include "../memory/rom.h"

#include <cassert>
#include <cstdint>

namespace N64 {
namespace Cpu {

constexpr std::array<std::string_view, 32> GPR_NAMES = {
    "zero", // 0
    "at",   // 1
    "v0",   // 2
    "v1",   // 3
    "a0",   // 4
    "a1",   // 5
    "a2",   // 6
    "a3",   // 7
    "t0",   // 8
    "t1",   // 9
    "t2",   // 10
    "t3",   // 11
    "t4",   // 12
    "t5",   // 13
    "t6",   // 14
    "t7",   // 15
    "s0",   // 16
    "s1",   // 17
    "s2",   // 18
    "s3",   // 19
    "s4",   // 20
    "s5",   // 21
    "s6",   // 22
    "s7",   // 23
    "t8",   // 24
    "t9",   // 25
    "k0",   // 26
    "k1",   // 27
    "gp",   // 28
    "sp",   // 29
    "s8",   // 30
    "ra"    // 31
};

class Gpr {
  private:
    std::array<uint64_t, 32> gpr{};

  public:
    uint64_t read(uint32_t reg_num) const {
        assert(reg_num < 32);
        if (reg_num == 0) {
            return 0;
        } else {
            return gpr[reg_num];
        }
    }

    void write(uint32_t reg_num, uint64_t value) {
        assert(reg_num < 32);
        if (reg_num != 0) {
            gpr[reg_num] = value;
        }
    }
};
} // namespace Cpu
} // namespace N64

#endif
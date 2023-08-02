#ifndef GPR_H
#define GPR_H

#include "../memory/rom.h"

#include <cassert>
#include <cstdint>

namespace N64 {
namespace Cpu {

// https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/Register.cpp#L9
constexpr std::array<std::string_view, 32> GPR_NAMES = {
    "R0", "AT", "V0", "V1", "A0", "A1", "A2", "A3", "T0", "T1", "T2",
    "T3", "T4", "T5", "T6", "T7", "S0", "S1", "S2", "S3", "S4", "S5",
    "S6", "S7", "T8", "T9", "K0", "K1", "GP", "SP", "FP", "RA",
};

class Gpr {
  private:
    std::array<uint64_t, 32> reg{};

  public:
    uint64_t read(uint32_t reg_num) const {
        assert(reg_num < 32);
        if (reg_num == 0) {
            return 0;
        } else {
            return reg[reg_num];
        }
    }

    void write(uint32_t reg_num, uint64_t value) {
        assert(reg_num < 32);
        if (reg_num != 0) {
            reg[reg_num] = value;
        }
    }
};
} // namespace Cpu
} // namespace N64

#endif
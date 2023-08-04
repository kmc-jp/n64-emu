#ifndef CPU_H
#define CPU_H

#include "cop0.h"
#include "instruction.h"
#include <cstdint>

namespace N64 {
namespace Cpu {

const uint32_t CPU_CYCLES_PER_INST = 1;

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

class Cpu {
  public:
    Gpr gpr;

    // special registers
    // 1.3
    // https://ultra64.ca/files/documentation/silicon-graphics/SGI_R4300_RISC_Processor_Specification_REV2.2.pdf
    uint64_t lo;
    uint64_t hi;

    // branch delay slot?
    bool delay_slot;
    bool prev_delay_slot;

    Cop0 cop0;

    // TODO: add COP1?

    Cpu() {}

    void reset();

    void dump();

    void set_pc64(uint64_t value);

    uint64_t get_pc64() const;

    // CPUの1ステップを実行する
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.c#L758
    void step();

    void execute_instruction(instruction_t inst);

    inline static Cpu &get_instance() { return instance; }

  private:
    // program counter
    uint64_t pc;
    uint64_t next_pc;

    // global instance
    static Cpu instance;

    // impl pattern
    class CpuImpl;
    class FpuImpl;
};

// https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/Register.cpp#L9
constexpr std::array<std::string_view, 32> GPR_NAMES = {
    "R0", "AT", "V0", "V1", "A0", "A1", "A2", "A3", "T0", "T1", "T2",
    "T3", "T4", "T5", "T6", "T7", "S0", "S1", "S2", "S3", "S4", "S5",
    "S6", "S7", "T8", "T9", "K0", "K1", "GP", "SP", "FP", "RA",
};

} // namespace Cpu

inline Cpu::Cpu &g_cpu() { return Cpu::Cpu::get_instance(); }

} // namespace N64

#endif
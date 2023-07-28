#ifndef CPU_H
#define CPU_H

#include "cop0.h"
#include "gpr.h"
#include "instruction.h"
#include <cstdint>

namespace N64 {
namespace Cpu {

const uint32_t CPU_CYCLES_PER_INST = 1;

class Cpu {
  private:
    // program counter
    uint64_t pc;
    uint64_t next_pc;

    static Cpu instance;

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

    void set_pc64(uint64_t value) {
        pc = value;
        next_pc = value + 4;
    }

    uint64_t get_pc64() const { return pc; }

    // CPUの1ステップを実行する
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.c#L758
    void step();

    void execute_instruction(instruction_t inst);

    inline static Cpu &get_instance() { return instance; }

    class Operation;
};

} // namespace Cpu

inline Cpu::Cpu &g_cpu() { return Cpu::Cpu::get_instance(); }

} // namespace N64

#endif
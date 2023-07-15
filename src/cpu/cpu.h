#ifndef CPU_H
#define CPU_H

#include "cop0.h"
#include "gpr.h"
#include "instruction.h"

#include <cstdint>
#include <spdlog/spdlog.h>

namespace N64 {
namespace Cpu {

const uint32_t CPU_CYCLES_PER_INST = 1;

class Cpu {
  private:
    // program counter
    uint64_t pc;
    uint64_t next_pc;
    // special registers
    // 1.3
    // https://ultra64.ca/files/documentation/silicon-graphics/SGI_R4300_RISC_Processor_Specification_REV2.2.pdf
    uint64_t lo;
    uint64_t hi;

    static Cpu instance;

  public:
    Gpr gpr;
    // branch delay slot?
    bool delay_slot;
    bool prev_delay_slot;

    Cop0 cop0;

    // TODO: add COP1?

    Cpu() : delay_slot(false), prev_delay_slot(false) {}

    void reset();

    void dump() {
        spdlog::info("======= Core dump =======");
        spdlog::info("PC\t= {:#x}", pc);
        for (int i = 0; i < 16; i++) {
            spdlog::info("{}\t= {:#018x}\t{}\t= {:#018x}", GPR_NAMES[i],
                         gpr.read(i), GPR_NAMES[i + 16], gpr.read(i + 16));
        }
        spdlog::info("");
        cop0.dump();
        spdlog::info("=========================");
    }

    void set_pc64(uint64_t value) {
        pc = value;
        next_pc = value + 4;
    }

    // CPUの1ステップを実行する
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.c#L758
    void step();

    void execute_instruction(instruction_t inst);

    static Cpu &get_instance() { return instance; }

    class Operation;
};

} // namespace Cpu

inline Cpu::Cpu &g_cpu() { return Cpu::Cpu::get_instance(); }

} // namespace N64

#endif
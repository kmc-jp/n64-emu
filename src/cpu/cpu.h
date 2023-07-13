#ifndef CPU_H
#define CPU_H

#include "cop0.h"
#include "gpr.h"
#include "instruction.h"
#include "rom.h"

#include <cstdint>
#include <spdlog/spdlog.h>

namespace N64 {
namespace Cpu {

const uint32_t CPU_CYCLES_PER_INST = 1;

const std::string GPR_NAMES[] = {
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

class Cpu {
  private:
    uint64_t pc;
    uint64_t next_pc;

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

    void branch_offset16(bool cond, instruction_t inst);
    void branch_likely_offset16(bool cond, instruction_t inst);
    void branch_addr64(bool cond, uint64_t vaddr);
    void branch_likely_addr64(bool cond, uint64_t vaddr);
};

} // namespace Cpu

extern Cpu::Cpu n64cpu;

} // namespace N64

#endif

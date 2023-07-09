#ifndef CPU_H
#define CPU_H

#include "cop0.h"
#include "instruction.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <spdlog/spdlog.h>

namespace N64 {
namespace Cpu {

const uint32_t CPU_CYCLES_PER_INST = 1;

class Cpu {
    class Gpr {
      private:
        uint64_t gpr[32];

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

        void dump() {
            for (int i = 0; i < 16; i++) {
                spdlog::info("GPR[{}]\t= 0x{:016x}\tGPR[{}]\t= 0x{:016x}", i,
                             gpr[i], i + 16, gpr[i + 16]);
            }
        }
    };

  public:
    uint64_t pc;
    Gpr gpr;

    // TODO: add delay slots

    Cop0 cop0;

    // TODO: add COP1?

    Cpu() {}

    void init() {
        // レジスタの初期化
        // FIXME: 必要ない?
        // COP0の初期化
        cop0.init();
    }

    void dump() {
        spdlog::info("======= Core dump =======");
        spdlog::info("PC = 0x{:x}", pc);
        gpr.dump();
        spdlog::info("");
        cop0.dump();
        spdlog::info("=========================");
    }

    // CPUの1ステップを実行する
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.c#L758
    void step();

    void execute_instruction(instruction_t inst);
};

} // namespace Cpu

extern Cpu::Cpu n64cpu;

} // namespace N64

#endif

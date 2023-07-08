#ifndef CPU_H
#define CPU_H

#include <cstdint>
#include <iostream>
#include <spdlog/spdlog.h>
#include "cop0.h"
#include "../mmu/mmu.h"

namespace N64 {

const int NUM_GPR = 32;

class Cpu {
  public:
    uint64_t pc;
    uint32_t gpr[NUM_GPR];

    Cop0 cop0;

    // TODO: COP1を追加

    Cpu() {}

    void init() {
        // レジスタの初期化
        pc = 0;
        for (int i = 0; i < NUM_GPR; i++) {
            gpr[i] = 0;
        }
        // COP0の初期化
        cop0.init();
    }

    void dump() {
        spdlog::info("======= Core dump =======");
        spdlog::info("PC = 0x{:x}", pc);
        for (int i = 0; i < NUM_GPR; i++) {
            spdlog::info("GPR[{}] = 0x{:x}", i, gpr[i]);
        }
        spdlog::info("=========================");
    }

    // CPUの1ステップを実行する
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.c#L758
    void step();
};

extern Cpu n64cpu;

} // namespace N64

#endif

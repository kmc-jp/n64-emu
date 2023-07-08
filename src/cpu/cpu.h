#ifndef CPU_H
#define CPU_H

#include <cstdint>
#include <iostream>
#include <spdlog/spdlog.h>
#include "cop0.h"

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
        spdlog::info("PC = 0x{:x}", pc);
        for (int i = 0; i < NUM_GPR; i++) {
            spdlog::info("GPR[{}] = 0x{:x}", i, gpr[i]);
        }
    }
};

extern Cpu n64cpu;

} // namespace N64

#endif

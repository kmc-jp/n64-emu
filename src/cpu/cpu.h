#ifndef CPU_H
#define CPU_H

#include <cstdint>
#include <iostream>
#include <spdlog/spdlog.h>

namespace N64 {

const int NUM_GPR = 32;

class Cpu {
  public:
    uint64_t pc;
    uint32_t gpr[NUM_GPR];

    Cpu() {}

    void init() {
        pc = 0;
        for (int i = 0; i < NUM_GPR; i++) {
            gpr[i] = 0;
        }
    }

    void dump() {
        spdlog::info("PC = 0x{:x}", pc);
        for (int i = 0; i < NUM_GPR; i++) {
            spdlog::info("GPR[{}] = 0x{:x}", i, gpr[i]);
        }
    }
};

extern Cpu n64cpu;

void foo();

} // namespace N64

#endif

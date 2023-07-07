#ifndef CPU_H
#define CPU_H

#include <cstdint>
#include <iostream>

namespace N64 {

const int NUM_GPR = 32;

class Cpu {
  public:
    uint64_t pc;
    uint32_t gpr[NUM_GPR];

    Cpu() : pc(0) {
        for (int i = 0; i < NUM_GPR; i++) {
            gpr[i] = 0;
        }
    }

    void dump() {
        std::cout << "PC\t= 0x" << std::hex << pc << std::endl;
        for (int i = 0; i < NUM_GPR; i++) {
            std::cout << "GPR[" << std::dec << i << "]\t= 0x" << std::hex
                      << gpr[i] << std::endl;
        }
    }
};

static Cpu n64cpu{};

} // namespace N64

#endif

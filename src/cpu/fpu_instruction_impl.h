#ifndef FPU_INSTRUCTIONS_CPP
#define FPU_INSTRUCTIONS_CPP

#include "cop0.h"
#include "cpu.h"
#include "gpr.h"
#include "instruction.h"
#include "memory/bus.h"
#include "memory/tlb.h"
#include "mmu/mmu.h"
#include "utils/utils.h"
#include <cstdint>
#include <optional>

namespace N64 {
namespace Cpu {

class Cpu::FpuImpl {
  public:
    static void op_cfc1(Cpu &cpu, instruction_t inst) {
        Utils::abort("CFC1: not implemented");
    }
};

} // namespace Cpu
} // namespace N64

#endif

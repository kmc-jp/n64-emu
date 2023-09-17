#ifndef FPU_INSTRUCTIONS_CPP
#define FPU_INSTRUCTIONS_CPP

#include "cpu/instruction.h"

namespace N64::Cpu {
class Cpu;
}

namespace N64 {
namespace Cpu {

class FpuImpl {
  private:
    static bool test_cop1_usable_exception(Cpu &cpu);

  public:
    static void op_cfc1(Cpu &cpu, instruction_t inst);

    static void op_ctc1(Cpu &cpu, instruction_t inst);
};

} // namespace Cpu
} // namespace N64

#endif

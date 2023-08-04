#ifndef CPU_OPERATION_H
#define CPU_OPERATION_H

#include "cpu.h"

namespace N64 {
namespace Cpu {

class Cpu::Operation {
  public:
    static void assert_encoding_is_valid(bool validity) {
        // should be able to ignore?
        assert(validity);
    }

    static void execute(Cpu &cpu, instruction_t inst);

  private:
    class CpuImpl;
};

} // namespace Cpu
} // namespace N64

#endif
﻿#ifndef CPU_OPERATION_H
#define CPU_OPERATION_H

#include "cpu.h"

namespace N64 {
namespace Cpu {

class Cpu::Operation {
  public:
    static void op_sub(Cpu &cpu, instruction_t inst);
    static void execute(Cpu &cpu, instruction_t inst);

  private:
    class Impl;
};

} // namespace Cpu
} // namespace N64

#endif
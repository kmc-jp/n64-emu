#ifndef CPU_CACHED_INTERP_H
#define CPU_CACHED_INTERP_H

#include "cpu/instruction.h"
#include <cstdint>

namespace N64 {
namespace Cpu {

class Cpu;

namespace CachedInterp {

using Handler = void (*)(Cpu &cpu, instruction_t inst);

struct CachedWord {
    uint32_t word{0};
    Handler handler{nullptr};
};

// Decode raw instruction to a CpuImpl/FpuImpl handler (no execute).
Handler decode(instruction_t inst);

void reset();
void invalidate_page(uint32_t paddr);
void invalidate_range(uint32_t paddr, uint32_t length);
void clear();

// One instruction; same semantics as Cpu::step (delay slot, IRQ, COUNT).
void step_one();

// Run up to `budget` instructions; advances RSP/scheduler internally.
int run(int budget, bool rsp_thread);

} // namespace CachedInterp
} // namespace Cpu
} // namespace N64

#endif

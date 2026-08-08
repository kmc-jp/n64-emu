#ifndef N64_SYSTEM_MACHINE_ADVANCE_H
#define N64_SYSTEM_MACHINE_ADVANCE_H

namespace N64 {
namespace N64System {

// After the CPU burns `cpu_cycles`, advance RSP (2/3 rate or kick worker)
// and the scheduler. Safe between dynarec basic blocks.
// cpu_cycles == 0 only flushes overdue scheduler events.
void advance_after_cpu(int cpu_cycles, bool rsp_thread);

} // namespace N64System
} // namespace N64

#endif

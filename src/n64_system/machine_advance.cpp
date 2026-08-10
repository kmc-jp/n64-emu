#include "n64_system/machine_advance.h"
#include "n64_system/scheduler.h"

namespace N64 {
namespace N64System {

void advance_after_cpu(int cpu_cycles) {
    if (cpu_cycles < 0)
        return;
    g_scheduler().tick(static_cast<uint64_t>(cpu_cycles));
}

} // namespace N64System
} // namespace N64

#include "n64_system/machine_advance.h"
#include "n64_system/scheduler.h"
#include "rcp/rsp.h"
#include "rcp/rsp_thread.h"

namespace N64 {
namespace N64System {

namespace {
int g_rsp_cpu_credit = 0;
}

void advance_after_cpu(int cpu_cycles, bool rsp_thread) {
    if (cpu_cycles < 0)
        return;

    if (cpu_cycles > 0) {
        if (rsp_thread) {
            if (!g_rsp().halted())
                Rsp::g_rsp_thread().kick_until_halt();
        } else {
            g_rsp_cpu_credit += cpu_cycles;
            auto &rsp = g_rsp();
            while (g_rsp_cpu_credit >= 3) {
                g_rsp_cpu_credit -= 3;
                rsp.step();
                rsp.step();
            }
        }
    }

    g_scheduler().tick(static_cast<uint64_t>(cpu_cycles));
}

} // namespace N64System
} // namespace N64

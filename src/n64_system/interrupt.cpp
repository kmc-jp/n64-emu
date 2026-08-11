#include "cpu/cpu.h"
#include "mmio/mi.h"

namespace N64 {
namespace N64System {

// Called when each interface updates its interrupt register?
void check_interrupt() {

    if (g_mi().get_reg_intr().raw & g_mi().get_reg_intr_mask().raw) {
        g_cpu().cop0.reg.cause.ip2 = 1;
    } else {
        g_cpu().cop0.reg.cause.ip2 = 0;
    }
}

} // namespace N64System
} // namespace N64

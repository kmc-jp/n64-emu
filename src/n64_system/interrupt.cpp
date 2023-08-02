#include "cpu/cpu.h"
#include "mmio/mi.h"

namespace N64 {
namespace N64System {

// 各インターフェースのinterruptレジスタ更新時に呼び出す?
void check_interrupt() {
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/Register.cpp#L502

    if (g_mi().get_reg_intr().raw & g_mi().get_reg_intr_mask().raw) {
        g_cpu().cop0.reg.cause.ip2 = 1;
    } else {
        g_cpu().cop0.reg.cause.ip2 = 0;
    }

// FIXME: remove this?
    if ((g_cpu().cop0.reg.cause.interrupt_pending &
         g_cpu().cop0.reg.status.im) != 0) {
        // FIXME: CPUの方だけでチェックすれば問題なさそう
        Utils::unimplemented("Interrupt occured!");
    }

    // TODO: pi reg_status
}

} // namespace N64System
} // namespace N64

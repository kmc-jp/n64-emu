#include "mmio/ai.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "utils/log.h"

namespace N64 {
namespace Mmio {
namespace AI {

void AI::reset() {
    Utils::debug("Resetting AI");
    // TODO: reset registers
}

uint32_t AI::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_AI_STATUS:
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/interface/ai.c#L58
    default: {
        Utils::critical("AI: Read from paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

void AI::write_paddr32(uint32_t paddr, uint32_t value) {
    switch (paddr) {
    case PADDR_AI_STATUS: {
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/AudioInterfaceHandler.cpp#L139
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/interface/ai.c#L30
        // https://github.com/SimoneN64/Kaizen/blob/74dccb6ac6a679acbf41b497151e08af6302b0e9/src/backend/core/mmio/AI.cpp#L23
        g_mi().get_reg_intr().ai = 0;
        N64System::check_interrupt();
        // FIXME: Need to update other registers?
    } break;
    default: {
        Utils::critical("AI: Write to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

AI &AI::get_instance() { return instance; }

AI AI::instance{};

} // namespace AI
} // namespace Mmio

Mmio::AI::AI &g_ai() { return Mmio::AI::AI::get_instance(); }

} // namespace N64

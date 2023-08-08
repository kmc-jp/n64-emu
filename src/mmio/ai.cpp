#include "ai.h"
#include "memory/memory_map.h"
#include "mi.h"
#include "n64_system/interrupt.h"
#include "utils/utils.h"
#include <cstdint>

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
        g_mi().get_reg_intr().ai = 0;
        // FIXME: Need to update other registers?
        N64System::check_interrupt();
    } break;
    default: {
        Utils::critical("AI: Write to paddr: {:#010x}", paddr);
        Utils::abort("Aborted");
    } break;
    }
}

AI AI::instance{};

} // namespace AI
} // namespace Mmio
} // namespace N64

#pragma once

#include <cstdint>

namespace Vulkan {
class WSI;
}
namespace N64::Mmio::VI {
class VI;
}

namespace N64 {
namespace PRDPWrapper {

void init_prdp(Vulkan::WSI &wsi, uint8_t *rdram);

void fini_prdp();

void update_screen(Vulkan::WSI &wsi, N64::Mmio::VI::VI &vi);

// Forward RDP command words to Parallel-RDP.
void enqueue_command(int command_length, const uint32_t *buffer);

void on_full_sync();

} // namespace PRDPWrapper
} // namespace N64

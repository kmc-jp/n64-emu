#pragma once

#include <cstdint>
#include <mutex>

namespace Vulkan {
class WSI;
}
namespace N64::Mmio::VI {
class VI;
}

namespace N64 {
namespace PRDPWrapper {

// Serializes Parallel-RDP / Vulkan use (RSP worker may submit DPC commands
// while the main thread presents).
std::recursive_mutex &rdp_mutex();

void init_prdp(Vulkan::WSI &wsi, uint8_t *rdram, unsigned upscale,
               bool frame_interp);

void fini_prdp();

void update_screen(Vulkan::WSI &wsi, N64::Mmio::VI::VI &vi);

// Forward RDP command words to Parallel-RDP.
void enqueue_command(int command_length, const uint32_t *buffer);

void on_full_sync();

// Present path counters for N64_PROFILE_FRAME (since last take, then reset).
struct PresentStats {
    uint64_t presented = 0;
    uint64_t skipped = 0;
};
PresentStats take_present_stats();

} // namespace PRDPWrapper
} // namespace N64

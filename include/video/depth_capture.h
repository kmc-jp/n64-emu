#pragma once

#include "device.hpp"
#include "image.hpp"
#include "rdp_renderer.hpp"
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace N64 {
namespace PRDPWrapper {

// Snapshots RDRAM Z at SyncFull (before the next frame clears a shared Z),
// keyed by the color framebuffer address for VI_ORIGIN lookup.
class DepthCapturer {
  public:
    DepthCapturer() = default;

    void reset();
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }

    // Called from the RDP command thread after SyncFull.
    void on_sync_full(const RDP::Renderer::DepthBufferInfo &info,
                      Vulkan::Device &device, Vulkan::Buffer &rdram,
                      size_t rdram_offset, size_t rdram_size);

    // Look up depth for a VI_ORIGIN (or SetColorImage addr). May be null.
    Vulkan::ImageHandle take(uint32_t color_addr);

    static void sync_full_thunk(void *userdata,
                                const RDP::Renderer::DepthBufferInfo &info,
                                Vulkan::Device &device, Vulkan::Buffer &rdram,
                                size_t rdram_offset, size_t rdram_size);

  private:
    bool enabled_ = false;
    bool programs_ready_ = false;
    Vulkan::Program *prog_extract_ = nullptr;
    std::mutex mu_;
    // Ring of a few recent color FBs (double/triple buffered titles).
    std::unordered_map<uint32_t, Vulkan::ImageHandle> by_color_;

    void ensure_program(Vulkan::Device &device);
};

} // namespace PRDPWrapper
} // namespace N64

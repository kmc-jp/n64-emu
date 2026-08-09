#pragma once

#include "image.hpp"
#include "rdp_renderer.hpp"
#include <cstdint>
#include <mutex>

namespace Vulkan {
class Device;
}

namespace N64 {
namespace Rdp {

std::recursive_mutex &mutex();

void init(Vulkan::Device &device, uint8_t *rdram, unsigned upscale);
void fini();
bool ready();

void enqueue_command(int command_length, const uint32_t *buffer);
void on_full_sync();

using SyncFullCallback = void (*)(void *userdata,
                                  const RDP::Renderer::DepthBufferInfo &info,
                                  Vulkan::Device &device, Vulkan::Buffer &rdram,
                                  size_t rdram_offset, size_t rdram_size);
void set_sync_full_callback(SyncFullCallback cb, void *userdata);

bool is_dirty();
void clear_dirty();

struct ViRegs {
    uint32_t status{};
    uint32_t origin{};
    uint32_t width{};
    uint32_t intr{};
    uint32_t current{};
    uint32_t burst{};
    uint32_t vsync{};
    uint32_t hsync{};
    uint32_t hsync_leap{};
    uint32_t h_video{};
    uint32_t v_video{};
    uint32_t v_burst{};
    uint32_t x_scale{};
    uint32_t y_scale{};
};

struct ScanoutResult {
    Util::IntrusivePtr<Vulkan::Image> image;
    uint32_t origin{0};
    bool skip{true};
};

ScanoutResult scanout(const ViRegs &vi);

} // namespace Rdp
} // namespace N64

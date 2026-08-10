#pragma once

#include "video/frame_interpolate.h"
#include "wsi.hpp"
#include <cstdint>

namespace Vulkan {
class CommandBuffer;
}
namespace N64::Mmio::VI {
class VI;
}

namespace N64 {
namespace Video {

struct PresentStats {
    uint64_t presented = 0;
    uint64_t skipped = 0;
};

void init_video(Vulkan::WSI &wsi, uint8_t *rdram, unsigned upscale,
                bool frame_interp);
void fini_video();
void reinit_rdp(Vulkan::WSI &wsi, uint8_t *rdram, unsigned upscale,
                bool frame_interp);

void set_frame_interp_enabled(bool enabled);
bool frame_interp_enabled();
void set_frame_interp_mode(FrameInterpMode mode);
FrameInterpMode frame_interp_mode();
// Last measured source hold (VI fields per novel frame). 1 if every field is novel.
unsigned frame_interp_pair_k();

bool present_field(Vulkan::WSI &wsi, N64::Mmio::VI::VI &vi,
                   bool force_present = false);
void present_ui_only(Vulkan::WSI &wsi);

// Swapchain clear color (menu background / letterbox). RGBA in [0,1].
void set_clear_color(float r, float g, float b, float a = 1.f);

PresentStats take_present_stats();

using OverlayDrawFn = void (*)(Vulkan::CommandBuffer &cmd);
void set_overlay_draw(OverlayDrawFn fn);

} // namespace Video
} // namespace N64

#include "video/present.h"
#include "fragment_spirv.h"
#include "mmio/vi.h"
#include "rdp/rdp_core.h"
#include "video/depth_capture.h"
#include "video/frame_interpolate.h"
#include "utils/log.h"
#include "vertex_spirv.h"
#include <mutex>

namespace N64 {
namespace Video {

namespace {
FrameInterpolator g_frame_interp;
DepthCapturer g_depth_capture;
bool g_have_presented_origin = false;
uint32_t g_last_presented_origin = 0;
uint64_t g_presents = 0;
uint64_t g_present_skips = 0;
OverlayDrawFn g_overlay_draw = nullptr;
float g_clear_color[4] = {0.f, 0.f, 0.f, 1.f};

void calculate_viewport(float *x, float *y, float *width, float *height,
                        float win_w, float win_h) {
    constexpr float kDisplayW = 640.f;
    constexpr float kDisplayH = 480.f;

    if (win_w <= 0.f || win_h <= 0.f) {
        *x = *y = 0.f;
        *width = win_w;
        *height = win_h;
        return;
    }

    *x = 0.f;
    *y = 0.f;
    *width = win_w;
    *height = win_h;

    const float hw = kDisplayH * win_w;
    const float wh = kDisplayW * win_h;
    if (hw > wh) {
        const float w_max = wh / kDisplayH;
        *x = (win_w - w_max) * 0.5f;
        *width = w_max;
    } else if (hw < wh) {
        const float h_max = hw / kDisplayW;
        *y = (win_h - h_max) * 0.5f;
        *height = h_max;
    }
}

void render_screen(Vulkan::WSI &wsi,
                   Util::IntrusivePtr<Vulkan::Image> image) {
    Vulkan::ResourceLayout vertex_layout = {};
    Vulkan::ResourceLayout fragment_layout = {};
    fragment_layout.output_mask = 1 << 0;
    fragment_layout.sets[0].sampled_image_mask = 1 << 0;
    fragment_layout.sets[0].fp_mask = 1 << 0;
    fragment_layout.sets[0].array_size[0] = 1;
    auto *program = wsi.get_device().request_program(
        vertex_spirv, sizeof(vertex_spirv), fragment_spirv,
        sizeof(fragment_spirv), &vertex_layout, &fragment_layout);

    Util::IntrusivePtr<Vulkan::CommandBuffer> cmd =
        wsi.get_device().request_command_buffer();
    Vulkan::RenderPassInfo rp = wsi.get_device().get_swapchain_render_pass(
        Vulkan::SwapchainRenderPass::ColorOnly);
    rp.clear_color[0].float32[0] = g_clear_color[0];
    rp.clear_color[0].float32[1] = g_clear_color[1];
    rp.clear_color[0].float32[2] = g_clear_color[2];
    rp.clear_color[0].float32[3] = g_clear_color[3];
    cmd->begin_render_pass(rp);
    if (image) {
        VkViewport vp = cmd->get_viewport();
        calculate_viewport(&vp.x, &vp.y, &vp.width, &vp.height, vp.width,
                           vp.height);

        VkRect2D scissor{};
        scissor.offset.x = static_cast<int32_t>(vp.x);
        scissor.offset.y = static_cast<int32_t>(vp.y);
        scissor.extent.width = static_cast<uint32_t>(vp.x + vp.width + 0.5f) -
                               static_cast<uint32_t>(scissor.offset.x);
        scissor.extent.height = static_cast<uint32_t>(vp.y + vp.height + 0.5f) -
                                static_cast<uint32_t>(scissor.offset.y);

        cmd->set_program(program);
        cmd->set_opaque_state();
        cmd->set_depth_test(false, false);
        cmd->set_cull_mode(VK_CULL_MODE_NONE);
        cmd->set_texture(0, 0, image->get_view(),
                         Vulkan::StockSampler::NearestClamp);
        cmd->set_viewport(vp);
        cmd->set_scissor(scissor);
        cmd->draw(3);
    }
    if (g_overlay_draw)
        g_overlay_draw(*cmd);
    cmd->end_render_pass();
    wsi.get_device().submit(cmd);
}

Rdp::ViRegs vi_to_regs(const N64::Mmio::VI::VI &vi) {
    Rdp::ViRegs r;
    r.status = vi.reg_status;
    r.origin = vi.reg_origin;
    r.width = vi.reg_width;
    r.intr = vi.reg_intr;
    r.current = vi.reg_current;
    r.burst = vi.reg_burst;
    r.vsync = vi.reg_vsync;
    r.hsync = vi.reg_hsync;
    r.hsync_leap = vi.reg_hsync_leap;
    r.h_video = vi.reg_h_video;
    r.v_video = vi.reg_v_video;
    r.v_burst = vi.reg_v_burst;
    r.x_scale = vi.reg_x_scale;
    r.y_scale = vi.reg_y_scale;
    return r;
}
} // namespace

void set_overlay_draw(OverlayDrawFn fn) { g_overlay_draw = fn; }

PresentStats take_present_stats() {
    PresentStats s{g_presents, g_present_skips};
    g_presents = 0;
    g_present_skips = 0;
    return s;
}

void init_video(Vulkan::WSI &wsi, uint8_t *rdram, unsigned upscale,
                bool frame_interp) {
    g_frame_interp.reset();
    g_frame_interp.set_enabled(frame_interp);
    g_depth_capture.reset();
    g_depth_capture.set_enabled(frame_interp);
    g_have_presented_origin = false;
    g_last_presented_origin = 0;
    g_presents = 0;
    g_present_skips = 0;
#if !N64_FRAME_INTERP
    if (frame_interp) {
        Utils::warn(
            "--frame-interp requested but build has no glslc/shaders; "
            "ignoring");
        g_frame_interp.set_enabled(false);
        g_depth_capture.set_enabled(false);
    }
#else
    if (frame_interp)
        Utils::info("Frame interpolation enabled (optical flow + depth)");
#endif
    Rdp::init(wsi.get_device(), rdram, upscale);
    Rdp::set_sync_full_callback(DepthCapturer::sync_full_thunk,
                                &g_depth_capture);
}

void fini_video() {
    g_frame_interp.reset();
    g_depth_capture.reset();
    g_have_presented_origin = false;
    g_last_presented_origin = 0;
    Rdp::fini();
}

void reinit_rdp(Vulkan::WSI &wsi, uint8_t *rdram, unsigned upscale,
                bool frame_interp) {
    fini_video();
    init_video(wsi, rdram, upscale, frame_interp);
}

void set_frame_interp_enabled(bool enabled) {
#if !N64_FRAME_INTERP
    if (enabled) {
        Utils::warn("frame interp unavailable in this build");
        enabled = false;
    }
#endif
    g_frame_interp.set_enabled(enabled);
    g_depth_capture.set_enabled(enabled);
    Rdp::set_sync_full_callback(
        enabled ? DepthCapturer::sync_full_thunk : nullptr,
        enabled ? static_cast<void *>(&g_depth_capture) : nullptr);
}

bool frame_interp_enabled() { return g_frame_interp.enabled(); }

void set_frame_interp_mode(FrameInterpMode mode) {
    g_frame_interp.set_mode(mode);
}

FrameInterpMode frame_interp_mode() { return g_frame_interp.mode(); }

bool present_field(Vulkan::WSI &wsi, N64::Mmio::VI::VI &vi,
                   bool force_present) {
    std::lock_guard lock(Rdp::mutex());

    const bool dirty = Rdp::is_dirty();
    const bool can_skip_dup =
        !force_present && !g_frame_interp.enabled() && !dirty &&
        g_have_presented_origin && vi.reg_origin == g_last_presented_origin;
    if (can_skip_dup) {
        ++g_present_skips;
        return false;
    }

    auto result = Rdp::scanout(vi_to_regs(vi));
    if (result.skip && !force_present) {
        ++g_present_skips;
        return false;
    }

    Util::IntrusivePtr<Vulkan::Image> image = result.image;
    wsi.begin_frame();
    if (image) {
        auto depth = g_depth_capture.take(result.origin);
        image = g_frame_interp.process(wsi.get_device(), image, result.origin,
                                       depth);
    }
    render_screen(wsi, image);
    wsi.end_frame();

    Rdp::clear_dirty();
    if (!result.skip) {
        g_have_presented_origin = true;
        g_last_presented_origin = result.origin;
    }
    ++g_presents;
    return true;
}

void present_ui_only(Vulkan::WSI &wsi) {
    wsi.begin_frame();
    render_screen(wsi, {});
    wsi.end_frame();
    ++g_presents;
}

void set_clear_color(float r, float g, float b, float a) {
    g_clear_color[0] = r;
    g_clear_color[1] = g;
    g_clear_color[2] = b;
    g_clear_color[3] = a;
}

} // namespace Video
} // namespace N64

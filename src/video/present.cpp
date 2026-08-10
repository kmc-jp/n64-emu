#include "video/present.h"
#include "fragment_spirv.h"
#include "mmio/vi.h"
#include "rdp/rdp_core.h"
#include "video/depth_capture.h"
#include "video/frame_interpolate.h"
#include "utils/log.h"
#include "vertex_spirv.h"
#include <chrono>
#include <cstdlib>
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
    const bool depth =
        frame_interp && frame_interp_uses_depth(g_frame_interp.mode());
    g_depth_capture.set_enabled(depth);
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
        Utils::info(
            "Frame interpolation enabled; "
            "blend/warp at 1/{}x then upscale",
            upscale);
#endif
    g_frame_interp.set_upscale(upscale);
    Rdp::init(wsi.get_device(), rdram, upscale);
    Rdp::set_sync_full_callback(depth ? DepthCapturer::sync_full_thunk : nullptr,
                                depth ? static_cast<void *>(&g_depth_capture)
                                      : nullptr);
}

void fini_video(Vulkan::Device &device) {
    // Drain RDP (and clear SyncFull callbacks) before dropping frame-interp /
    // depth images that the command ring may still reference.
    Rdp::fini();
    device.wait_idle();
    g_frame_interp.reset();
    g_depth_capture.reset();
    g_have_presented_origin = false;
    g_last_presented_origin = 0;
}

void reinit_rdp(Vulkan::WSI &wsi, uint8_t *rdram, unsigned upscale,
                bool frame_interp) {
    fini_video(wsi.get_device());
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
    const bool depth =
        enabled && frame_interp_uses_depth(g_frame_interp.mode());
    g_depth_capture.set_enabled(depth);
    Rdp::set_sync_full_callback(
        depth ? DepthCapturer::sync_full_thunk : nullptr,
        depth ? static_cast<void *>(&g_depth_capture) : nullptr);
}

bool frame_interp_enabled() { return g_frame_interp.enabled(); }

void set_frame_interp_mode(FrameInterpMode mode) {
    g_frame_interp.set_mode(mode);
    if (g_frame_interp.enabled()) {
        const bool depth = frame_interp_uses_depth(mode);
        g_depth_capture.set_enabled(depth);
        Rdp::set_sync_full_callback(
            depth ? DepthCapturer::sync_full_thunk : nullptr,
            depth ? static_cast<void *>(&g_depth_capture) : nullptr);
    }
}

FrameInterpMode frame_interp_mode() { return g_frame_interp.mode(); }

unsigned frame_interp_pair_k() { return g_frame_interp.pair_k(); }

bool present_field(Vulkan::WSI &wsi, N64::Mmio::VI::VI &vi,
                   bool force_present) {
    using clock = std::chrono::steady_clock;
    static const bool profile = [] {
        const char *e = getenv("N64_PROFILE_PRESENT");
        if (!e || e[0] == '\0')
            e = getenv("N64_PROFILE_FRAME");
        return e && e[0] != '\0' && e[0] != '0';
    }();
    static uint64_t prof_fields = 0;
    static double prof_scanout_ms = 0.0;
    static double prof_acquire_ms = 0.0;
    static double prof_interp_ms = 0.0;
    static double prof_blit_ms = 0.0;
    static double prof_submit_ms = 0.0;
    static auto prof_last_log = clock::now();
    const auto stamp = [&] { return profile ? clock::now() : clock::time_point{}; };
    const auto elapsed_ms = [](clock::time_point a, clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    Rdp::ScanoutResult result;
    {
        std::lock_guard lock(Rdp::mutex());

        const bool dirty = Rdp::is_dirty();
        const bool can_skip_dup =
            !force_present && !g_frame_interp.enabled() && !dirty &&
            g_have_presented_origin && vi.reg_origin == g_last_presented_origin;
        if (can_skip_dup) {
            ++g_present_skips;
            return false;
        }

        const auto t_scanout = stamp();
        result = Rdp::scanout(vi_to_regs(vi));
        if (profile)
            prof_scanout_ms += elapsed_ms(t_scanout, stamp());
        if (result.skip && !force_present) {
            ++g_present_skips;
            return false;
        }
        Rdp::clear_dirty();
    }

    Util::IntrusivePtr<Vulkan::Image> image = result.image;
    const auto t_acquire = std::chrono::steady_clock::now();
    if (!wsi.begin_frame())
        return false;
    const double acquire_ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - t_acquire)
                                  .count();
    g_frame_interp.note_present_acquire_ms(acquire_ms);

    const auto t_interp = stamp();
    if (image) {
        auto depth = g_depth_capture.take(result.origin);
        image = g_frame_interp.process(wsi.get_device(), image, result.origin,
                                       depth);
    }
    const auto t_blit = stamp();
    render_screen(wsi, image);
    const auto t_submit = stamp();
    wsi.end_frame();
    const auto t_done = stamp();

    if (profile) {
        ++prof_fields;
        prof_acquire_ms += acquire_ms;
        prof_interp_ms += elapsed_ms(t_interp, t_blit);
        prof_blit_ms += elapsed_ms(t_blit, t_submit);
        prof_submit_ms += elapsed_ms(t_submit, t_done);
        if (elapsed_ms(prof_last_log, t_done) >= 1000.0) {
            const double inv = 1.0 / double(prof_fields);
            const auto it = g_frame_interp.take_timings();
            Utils::info(
                "present profile: fields/s={} avg scanout={:.2f}ms "
                "acquire={:.2f}ms interp={:.2f}ms (fence={:.2f}ms "
                "flow={:.2f}ms warp={:.2f}ms) blit={:.2f}ms present={:.2f}ms "
                "total={:.2f}ms | flows/s={} warps/s={} fallback/s={}{}",
                prof_fields, prof_scanout_ms * inv, prof_acquire_ms * inv,
                prof_interp_ms * inv, it.fence_wait_ms * inv, it.flow_ms * inv,
                it.warp_ms * inv,
                prof_blit_ms * inv, prof_submit_ms * inv,
                (prof_scanout_ms + prof_acquire_ms + prof_interp_ms +
                 prof_blit_ms + prof_submit_ms) *
                    inv,
                it.flows, it.warps, it.fallback_fields,
                g_frame_interp.fallback_active() ? " [fallback]" : "");
            prof_fields = 0;
            prof_scanout_ms = 0.0;
            prof_acquire_ms = 0.0;
            prof_interp_ms = 0.0;
            prof_blit_ms = 0.0;
            prof_submit_ms = 0.0;
            prof_last_log = t_done;
        }
    }

    if (!result.skip) {
        g_have_presented_origin = true;
        g_last_presented_origin = result.origin;
    }
    ++g_presents;
    return true;
}

bool present_ui_only(Vulkan::WSI &wsi) {
    if (!wsi.begin_frame())
        return false;
    render_screen(wsi, {});
    wsi.end_frame();
    ++g_presents;
    return true;
}

void set_clear_color(float r, float g, float b, float a) {
    g_clear_color[0] = r;
    g_clear_color[1] = g;
    g_clear_color[2] = b;
    g_clear_color[3] = a;
}

} // namespace Video
} // namespace N64

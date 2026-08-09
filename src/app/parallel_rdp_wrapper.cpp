#include "app/parallel_rdp_wrapper.h"
#include "video/frame_interpolate.h"
#include "video/depth_capture.h"
#include "fragment_spirv.h"
#include "mmio/vi.h"
#include "rdp_device.hpp"
#include "utils/log.h"
#include "vertex_spirv.h"
#include "wsi.hpp"
#include <mutex>

namespace N64 {
namespace PRDPWrapper {

constexpr uint32_t RDRAM_SIZE = 8 * 1024 * 1024;
constexpr uint32_t HIDDEN_RDRAM_SIZE = 4 * 1024 * 1024;

RDP::CommandProcessor *command_processor;
FrameInterpolator g_frame_interp;
DepthCapturer g_depth_capture;

bool g_rdp_dirty = true;
bool g_have_presented_origin = false;
uint32_t g_last_presented_origin = 0;
uint64_t g_presents = 0;
uint64_t g_present_skips = 0;

std::recursive_mutex &rdp_mutex() {
    static std::recursive_mutex mu;
    return mu;
}

PresentStats take_present_stats() {
    std::lock_guard<std::recursive_mutex> lock(rdp_mutex());
    PresentStats s{g_presents, g_present_skips};
    g_presents = 0;
    g_present_skips = 0;
    return s;
}

void init_prdp(Vulkan::WSI &wsi, uint8_t *rdram, unsigned upscale,
               bool frame_interp) {
    std::lock_guard<std::recursive_mutex> lock(rdp_mutex());
    g_frame_interp.reset();
    g_frame_interp.set_enabled(frame_interp);
    g_depth_capture.reset();
    g_depth_capture.set_enabled(frame_interp);
    g_rdp_dirty = true;
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
    RDP::CommandProcessorFlags flags = 0;
    switch (upscale) {
    case 2:
        flags = RDP::COMMAND_PROCESSOR_FLAG_UPSCALING_2X_BIT;
        break;
    case 4:
        flags = RDP::COMMAND_PROCESSOR_FLAG_UPSCALING_4X_BIT;
        break;
    case 8:
        flags = RDP::COMMAND_PROCESSOR_FLAG_UPSCALING_8X_BIT;
        break;
    case 1:
        break;
    default:
        Utils::warn("Unsupported RDP upscale factor {}; using 1x", upscale);
        break;
    }

    // RDRAM must be host-endian word storage (byte addr^3 / half^2), which is
    // what paraLLEl-RDP shaders assume. See Utils::{byte,half}_address.
    auto aligned_rdram = reinterpret_cast<uintptr_t>(rdram);
    uintptr_t offset = 0;

    if (wsi.get_device().get_device_features().supports_external_memory_host) {
        size_t align =
            wsi.get_device()
                .get_device_features()
                .host_memory_properties.minImportedHostPointerAlignment;
        offset = aligned_rdram & (align - 1);
        aligned_rdram -= offset;
    }
    // FIXME: should align rdram??
    command_processor = new RDP::CommandProcessor(
        wsi.get_device(), reinterpret_cast<void *>(aligned_rdram), offset,
        RDRAM_SIZE, HIDDEN_RDRAM_SIZE, flags);

    if (!command_processor->device_is_supported()) {
        Utils::critical("Parallel-RDP does not support this device. Sorry!");
        exit(-1);
    }

    command_processor->set_sync_full_callback(DepthCapturer::sync_full_thunk,
                                              &g_depth_capture);
}

void fini_prdp() {
    std::lock_guard<std::recursive_mutex> lock(rdp_mutex());
    g_frame_interp.reset();
    g_depth_capture.reset();
    g_rdp_dirty = true;
    g_have_presented_origin = false;
    g_last_presented_origin = 0;
    if (command_processor)
        command_processor->set_sync_full_callback(nullptr, nullptr);
    delete command_processor;
    command_processor = nullptr;
}

constexpr RDP::ScanoutOptions get_prdp_scanout_options() {
    RDP::ScanoutOptions opts;
    opts.persist_frame_on_invalid_input = true;
    // Progressive window: upscale-deinterlace (defaults).
    opts.vi.aa = true;
    opts.vi.scale = true;
    opts.vi.dither_filter = true;
    opts.vi.divot_filter = true;
    opts.vi.gamma_dither = true;
    // 0 = native VI resolution; a bool true was wrongly promoting to 1 step.
    opts.downscale_steps = 0;
    opts.crop_overscan_pixels = 0;
    return opts;
}

// Fit into the swapchain using NTSC 4:3 display aspect — not the raw scanout
// pixel size.
static void calculate_viewport(float *x, float *y, float *width, float *height,
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
        // Pillarbox
        const float w_max = wh / kDisplayH;
        *x = (win_w - w_max) * 0.5f;
        *width = w_max;
    } else if (hw < wh) {
        // Letterbox
        const float h_max = hw / kDisplayW;
        *y = (win_h - h_max) * 0.5f;
        *height = h_max;
    }
}

void render_screen(Vulkan::WSI &wsi, Util::IntrusivePtr<Vulkan::Image> image) {
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
    cmd->begin_render_pass(rp);
    if (image.get() != NULL) {
        VkViewport vp = cmd->get_viewport();
        calculate_viewport(&vp.x, &vp.y, &vp.width, &vp.height, vp.width,
                           vp.height);

        VkRect2D scissor{};
        scissor.offset.x = static_cast<int32_t>(vp.x);
        scissor.offset.y = static_cast<int32_t>(vp.y);
        // Ceil extent so fractional viewport edges are not clipped short.
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
    cmd->end_render_pass();
    wsi.get_device().submit(cmd);
}
void update_screen(Vulkan::WSI &wsi, N64::Mmio::VI::VI &vi) {
    std::lock_guard<std::recursive_mutex> lock(rdp_mutex());
    uint32_t h_video = vi.reg_h_video;
    // H_START=H_END=0 blanks the VI (n64brew). Some titles leave it blank
    // until osViSetMode; use NTSC defaults so scanout is not invalid.
    const uint32_t h_start = (h_video >> 16) & 0x3ff;
    const uint32_t h_end = h_video & 0x3ff;
    if (h_start == 0 && h_end == 0 && vi.reg_width != 0 && vi.reg_origin != 0 &&
        vi.reg_origin != 0x280) {
        static bool logged = false;
        if (!logged) {
            Utils::warn(
                "VI_H_VIDEO is blank (0); using NTSC default 0x006c02ec "
                "for scanout (origin={:#x})",
                vi.reg_origin);
            logged = true;
        }
        h_video = 0x006c02ec;
    }

    command_processor->set_vi_register(RDP::VIRegister::Control, vi.reg_status);
    command_processor->set_vi_register(RDP::VIRegister::Origin, vi.reg_origin);
    command_processor->set_vi_register(RDP::VIRegister::Width, vi.reg_width);
    command_processor->set_vi_register(RDP::VIRegister::Intr, vi.reg_intr);
    command_processor->set_vi_register(RDP::VIRegister::VCurrentLine,
                                       vi.reg_current);
    command_processor->set_vi_register(RDP::VIRegister::Timing, vi.reg_burst);
    command_processor->set_vi_register(RDP::VIRegister::VSync, vi.reg_vsync);
    command_processor->set_vi_register(RDP::VIRegister::HSync, vi.reg_hsync);
    command_processor->set_vi_register(RDP::VIRegister::Leap,
                                       vi.reg_hsync_leap);
    command_processor->set_vi_register(RDP::VIRegister::HStart, h_video);
    command_processor->set_vi_register(RDP::VIRegister::VStart, vi.reg_v_video);
    command_processor->set_vi_register(RDP::VIRegister::VBurst, vi.reg_v_burst);
    command_processor->set_vi_register(RDP::VIRegister::XScale, vi.reg_x_scale);
    command_processor->set_vi_register(RDP::VIRegister::YScale, vi.reg_y_scale);

    // Skip present while VI still points at the boot placeholder framebuffer.
    if (vi.reg_origin == 0 || vi.reg_origin == 0x280) {
        return;
    }

    // Without frame interp, duplicate VI fields (same origin, no new RDP) only
    // repeat scanout/present and stall the GPU path — skip them.
    const bool can_skip_dup =
        !g_frame_interp.enabled() && !g_rdp_dirty && g_have_presented_origin &&
        vi.reg_origin == g_last_presented_origin;
    if (can_skip_dup) {
        ++g_present_skips;
        return;
    }

    //  FIXME: quarks?
    // https://github.com/simple64/simple64/blob/1e4ab555054a659c6e6a91db16ce46714be7ac00/parallel-rdp-standalone/parallel_imp.cpp#L257C7-L257C7

    // Do NOT call CommandProcessor::begin_frame_context() here. That path does
    // flush + drain + next_frame_context, and wsi.begin_frame() already advances
    // the frame context.
    RDP::ScanoutOptions opts = get_prdp_scanout_options();
    Util::IntrusivePtr<Vulkan::Image> image = command_processor->scanout(opts);

    wsi.begin_frame();
    if (image) {
        auto depth = g_depth_capture.take(vi.reg_origin);
        image = g_frame_interp.process(wsi.get_device(), image, vi.reg_origin,
                                       depth);
    }
    render_screen(wsi, image);
    wsi.end_frame();

    g_rdp_dirty = false;
    g_have_presented_origin = true;
    g_last_presented_origin = vi.reg_origin;
    ++g_presents;
}

void enqueue_command(int command_length, const uint32_t *buffer) {
    std::lock_guard<std::recursive_mutex> lock(rdp_mutex());
    if (!command_processor)
        return;
    g_rdp_dirty = true;
    command_processor->enqueue_command(command_length, buffer);
}

void on_full_sync() {
    std::lock_guard<std::recursive_mutex> lock(rdp_mutex());
    if (!command_processor)
        return;
    command_processor->wait_for_timeline(command_processor->signal_timeline());
}

} // namespace PRDPWrapper
} // namespace N64

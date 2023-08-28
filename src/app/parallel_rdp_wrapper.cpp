#include "spirv.h"
#include <mmio/vi.h>
#include <rdp_device.hpp>
#include <utils/utils.h>
#include <wsi.hpp>

namespace N64 {
namespace PRDPWrapper {

constexpr uint32_t RDRAM_SIZE = 8 * 1024 * 1024;
constexpr uint32_t HIDDEN_RDRAM_SIZE = 4 * 1024 * 1024;

RDP::CommandProcessor *command_processor;

void init_prdp(Vulkan::WSI &wsi, uint8_t *rdram) {
    RDP::CommandProcessorFlags flags = 0;

    // FIXME: I have no idea why these are needed
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
}

void fini_prdp() {
    delete command_processor;
    command_processor = nullptr;
}

constexpr RDP::ScanoutOptions get_prdp_scanout_options() {
    RDP::ScanoutOptions opts;
    opts.persist_frame_on_invalid_input = true;
    // anti-alias
    opts.vi.aa = true;
    opts.vi.scale = true;
    opts.vi.dither_filter = true;
    opts.vi.divot_filter = true;
    opts.vi.gamma_dither = true;
    opts.downscale_steps = true;
    opts.crop_overscan_pixels = true;
    return opts;
}

// https://github.com/simple64/simple64/blob/1e4ab555054a659c6e6a91db16ce46714be7ac00/parallel-rdp-standalone/parallel_imp.cpp#L154
static void calculate_viewport(float *x, float *y, float *width,
                               float *height) {
    int32_t display_width = 640;
    int32_t display_height = 480;

    *width = 800;
    *height = 600;
    *x = 0;
    *y = 0;
    int32_t hw = display_height * *width;
    int32_t wh = display_width * *height;

    // add letterboxes or pillarboxes if the window has a different aspect ratio
    // than the current display mode
    if (hw > wh) {
        int32_t w_max = wh / display_height;
        *x += (*width - w_max) / 2;
        *width = w_max;
    } else if (hw < wh) {
        int32_t h_max = hw / display_width;
        *y += (*height - h_max) / 2;
        *height = h_max;
    }
}

// https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/external/parallel-rdp/ParallelRDPWrapper.cpp#L228C91-L228C91
void render_screen(Vulkan::WSI &wsi, Util::IntrusivePtr<Vulkan::Image> image) {
    // https://github.com/simple64/simple64/blob/1e4ab555054a659c6e6a91db16ce46714be7ac00/parallel-rdp-standalone/parallel_imp.cpp#L199
    Vulkan::ResourceLayout vertex_layout = {};
    Vulkan::ResourceLayout fragment_layout = {};
    fragment_layout.output_mask = 1 << 0;
    fragment_layout.sets[0].sampled_image_mask = 1 << 0;
    auto *program = wsi.get_device().request_program(
        vertex_spirv, sizeof(vertex_spirv), fragment_spirv,
        sizeof(fragment_spirv), &vertex_layout, &fragment_layout);

    Util::IntrusivePtr<Vulkan::CommandBuffer> cmd =
        wsi.get_device().request_command_buffer();
    Vulkan::RenderPassInfo rp = wsi.get_device().get_swapchain_render_pass(
        Vulkan::SwapchainRenderPass::ColorOnly);
    cmd->begin_render_pass(rp);
    // https://github.com/simple64/simple64/blob/1e4ab555054a659c6e6a91db16ce46714be7ac00/parallel-rdp-standalone/parallel_imp.cpp#L211
    if (image.get() != NULL) {
        // TODO: remove from here...
        // rp.clear_color[0].float32[0] = 0.1f;
        // rp.clear_color[0].float32[1] = 0.2f;
        // rp.clear_color[0].float32[2] = 0.3f;
        // ...to here

        VkViewport vp = cmd->get_viewport();
        calculate_viewport(&vp.x, &vp.y, &vp.width, &vp.height);
        // Utils::debug("Viewport x: {}, y: {}, width: {}, height: {}",
        // vp.x, vp.y,
        //              vp.width, vp.height);
        cmd->set_program(program);
        cmd->set_opaque_state();
        cmd->set_depth_test(false, false);
        cmd->set_cull_mode(VK_CULL_MODE_NONE);
        cmd->set_texture(0, 0, image->get_view(),
                         Vulkan::StockSampler::NearestClamp);
        cmd->draw(3);
    }
    cmd->end_render_pass();
    wsi.get_device().submit(cmd);
}
void update_screen(Vulkan::WSI &wsi, N64::Mmio::VI::VI &vi) {
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
    command_processor->set_vi_register(RDP::VIRegister::HStart, vi.reg_h_video);
    command_processor->set_vi_register(RDP::VIRegister::VStart, vi.reg_v_video);
    command_processor->set_vi_register(RDP::VIRegister::VBurst, vi.reg_v_burst);
    command_processor->set_vi_register(RDP::VIRegister::XScale, vi.reg_x_scale);
    command_processor->set_vi_register(RDP::VIRegister::YScale, vi.reg_y_scale);

    //  FIXME: quarks?
    // https://github.com/simple64/simple64/blob/1e4ab555054a659c6e6a91db16ce46714be7ac00/parallel-rdp-standalone/parallel_imp.cpp#L257C7-L257C7

    RDP::ScanoutOptions opts = get_prdp_scanout_options();
    Util::IntrusivePtr<Vulkan::Image> image = command_processor->scanout(opts);

    command_processor->begin_frame_context();
    wsi.begin_frame();
    render_screen(wsi, image);
    wsi.end_frame();
}

} // namespace PRDPWrapper
} // namespace N64

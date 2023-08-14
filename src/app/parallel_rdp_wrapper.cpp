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
    opts.vi.aa = true;
    opts.vi.scale = true;
    opts.vi.dither_filter = true;
    opts.vi.divot_filter = true;
    opts.vi.gamma_dither = true;
    opts.downscale_steps = true;
    opts.crop_overscan_pixels = true;
    return opts;
}

// https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/external/parallel-rdp/ParallelRDPWrapper.cpp#L228C91-L228C91
void render_screen(Vulkan::WSI &wsi, Util::IntrusivePtr<Vulkan::Image> image) {
    wsi.begin_frame();

    if (!image) {
        auto info = Vulkan::ImageCreateInfo::immutable_2d_image(
            800, 600, VK_FORMAT_R8G8B8A8_UNORM);
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.misc = Vulkan::IMAGE_MISC_MUTABLE_SRGB_BIT;
        info.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        image = wsi.get_device().create_image(info);

        auto cmd = wsi.get_device().request_command_buffer();

        cmd->image_barrier(*image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_ACCESS_TRANSFER_WRITE_BIT);
        cmd->clear_image(*image, {});
        cmd->image_barrier(
            *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        wsi.get_device().submit(cmd);
    }

    Util::IntrusivePtr<Vulkan::CommandBuffer> cmd =
        wsi.get_device().request_command_buffer();
    cmd->begin_render_pass(wsi.get_device().get_swapchain_render_pass(
        Vulkan::SwapchainRenderPass::ColorOnly));
    cmd->end_render_pass();
    wsi.get_device().submit(cmd);
    wsi.end_frame();
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

    RDP::ScanoutOptions opts = get_prdp_scanout_options();
    Util::IntrusivePtr<Vulkan::Image> image = command_processor->scanout(opts);

    render_screen(wsi, image);
    command_processor->begin_frame_context();
}

} // namespace PRDPWrapper
} // namespace N64

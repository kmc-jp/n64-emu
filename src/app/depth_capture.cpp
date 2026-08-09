#include "app/depth_capture.h"
#include "utils/log.h"

#ifndef N64_FRAME_INTERP
#define N64_FRAME_INTERP 0
#endif

#if N64_FRAME_INTERP
#include "depth_extract_spirv.h"
#endif

namespace N64 {
namespace PRDPWrapper {
namespace {

Vulkan::ImageHandle create_depth_image(Vulkan::Device &device, unsigned w,
                                       unsigned h) {
    Vulkan::ImageCreateInfo info;
    info.domain = Vulkan::ImageDomain::Physical;
    info.width = w;
    info.height = h;
    info.depth = 1;
    info.levels = 1;
    info.layers = 1;
    info.format = VK_FORMAT_R16_SFLOAT;
    info.type = VK_IMAGE_TYPE_2D;
    info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
    auto img = device.create_image(info);
    img->set_layout(Vulkan::Layout::General);
    return img;
}

void fill_array_sizes(Vulkan::ResourceLayout &layout) {
    for (unsigned set = 0; set < Vulkan::VULKAN_NUM_DESCRIPTOR_SETS; ++set) {
        uint32_t mask = layout.sets[set].sampled_image_mask |
                        layout.sets[set].storage_image_mask |
                        layout.sets[set].storage_buffer_mask |
                        layout.sets[set].uniform_buffer_mask |
                        layout.sets[set].sampler_mask |
                        layout.sets[set].separate_image_mask;
        for (unsigned b = 0; b < Vulkan::VULKAN_NUM_BINDINGS; ++b) {
            if (mask & (1u << b))
                layout.sets[set].array_size[b] = 1;
        }
    }
}

} // namespace

void DepthCapturer::reset() {
    std::lock_guard<std::mutex> lock(mu_);
    by_color_.clear();
}

void DepthCapturer::ensure_program(Vulkan::Device &device) {
#if !N64_FRAME_INTERP
    (void)device;
#else
    if (programs_ready_)
        return;
    Vulkan::ResourceLayout layout = {};
    layout.sets[0].storage_buffer_mask = 1u << 0;
    layout.sets[0].storage_image_mask = 1u << 1;
    layout.sets[0].fp_mask = 1u << 1;
    layout.push_constant_size = 16;
    fill_array_sizes(layout);
    prog_extract_ = device.request_program(depth_extract_spirv,
                                            sizeof(depth_extract_spirv), &layout);
    programs_ready_ = prog_extract_ != nullptr;
    if (!programs_ready_)
        Utils::warn("DepthCapturer: failed to create extract program");
#endif
}

void DepthCapturer::sync_full_thunk(void *userdata,
                                    const RDP::Renderer::DepthBufferInfo &info,
                                    Vulkan::Device &device,
                                    Vulkan::Buffer &rdram, size_t rdram_offset,
                                    size_t rdram_size) {
    if (!userdata)
        return;
    static_cast<DepthCapturer *>(userdata)->on_sync_full(
        info, device, rdram, rdram_offset, rdram_size);
}

void DepthCapturer::on_sync_full(const RDP::Renderer::DepthBufferInfo &info,
                                 Vulkan::Device &device, Vulkan::Buffer &rdram,
                                 size_t rdram_offset, size_t rdram_size) {
#if !N64_FRAME_INTERP
    (void)info;
    (void)device;
    (void)rdram;
    (void)rdram_offset;
    (void)rdram_size;
    return;
#else
    if (!enabled_ || !info.valid())
        return;
    if (info.width > 1024 || info.height > 1024)
        return;

    ensure_program(device);
    if (!programs_ready_)
        return;

    auto img = create_depth_image(device, info.width, info.height);
    auto cmd = device.request_command_buffer();

    struct Push {
        uint32_t w, h;
        uint32_t depth_addr;
        uint32_t rdram_size;
    } push{info.width, info.height, info.depth_addr,
           static_cast<uint32_t>(rdram_size)};

    cmd->set_program(prog_extract_);
    cmd->set_storage_buffer(0, 0, rdram, rdram_offset, rdram_size);
    cmd->set_storage_texture(0, 1, img->get_view());
    cmd->push_constants(&push, 0, sizeof(push));
    cmd->dispatch((info.width + 7) / 8, (info.height + 7) / 8, 1);
    cmd->image_barrier(
        *img, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

    device.submit(cmd);

    const uint32_t key = info.color_addr & 0x00ffffffu;
    std::lock_guard<std::mutex> lock(mu_);
    by_color_[key] = img;
    // Bound growth: drop oldest-ish entries when many unique FBs appear.
    if (by_color_.size() > 8) {
        auto it = by_color_.begin();
        if (it->first != key)
            by_color_.erase(it);
    }
#endif
}

Vulkan::ImageHandle DepthCapturer::take(uint32_t color_addr) {
    const uint32_t key = color_addr & 0x00ffffffu;
    std::lock_guard<std::mutex> lock(mu_);
    auto it = by_color_.find(key);
    if (it == by_color_.end())
        return {};
    return it->second;
}

} // namespace PRDPWrapper
} // namespace N64

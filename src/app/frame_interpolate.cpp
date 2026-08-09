#include "app/frame_interpolate.h"
#include "utils/log.h"
#include "limits.hpp"

#ifndef N64_FRAME_INTERP
#define N64_FRAME_INTERP 0
#endif

#if N64_FRAME_INTERP
#include "flow_refine_spirv.h"
#include "flow_smooth_spirv.h"
#include "luma_downsample_spirv.h"
#include "pyramid_down_spirv.h"
#include "scene_finalize_spirv.h"
#include "scene_reduce_spirv.h"
#include "warp_blend_spirv.h"
#endif

#include <algorithm>
#include <cstdlib>

namespace N64 {
namespace PRDPWrapper {
namespace {

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

Vulkan::ImageHandle create_storage_image(Vulkan::Device &device, unsigned w,
                                         unsigned h, VkFormat format) {
    Vulkan::ImageCreateInfo info;
    info.domain = Vulkan::ImageDomain::Physical;
    info.width = w;
    info.height = h;
    info.depth = 1;
    info.levels = 1;
    info.layers = 1;
    info.format = format;
    info.type = VK_IMAGE_TYPE_2D;
    info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
    auto img = device.create_image(info);
    // Stay in GENERAL so sampled+storage descriptors agree with the real layout.
    img->set_layout(Vulkan::Layout::General);
    return img;
}

void storage_barrier(Vulkan::CommandBuffer &cmd, Vulkan::Image &img) {
    cmd.image_barrier(
        img, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

} // namespace

void FrameInterpolator::reset() {
    have_origin_ = false;
    last_origin_ = 0;
    hold_count_ = 0;
    consecutive_k1_ = 0;
    prev_novel_.reset();
    curr_novel_.reset();
    queue_.clear();
    scan_w_ = scan_h_ = 0;
    num_levels_ = 0;
    for (unsigned i = 0; i < kMaxLevels; ++i) {
        luma_a_[i].reset();
        luma_b_[i].reset();
        flow_ab_[i].reset();
        flow_ba_[i].reset();
        luma_w_[i] = luma_h_[i] = 0;
    }
    flow_tmp_.reset();
    output_.reset();
    scene_buf_.reset();
}

void FrameInterpolator::dispatch_2d(Vulkan::CommandBuffer &cmd, unsigned w,
                                    unsigned h) {
    cmd.dispatch((w + 7) / 8, (h + 7) / 8, 1);
}

void FrameInterpolator::ensure_programs(Vulkan::Device &device) {
#if !N64_FRAME_INTERP
    (void)device;
#else
    if (programs_ready_)
        return;

    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].sampled_image_mask = 1u << 0;
        layout.sets[0].storage_image_mask = 1u << 1;
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 8;
        fill_array_sizes(layout);
        prog_luma_ = device.request_program(luma_downsample_spirv,
                                            sizeof(luma_downsample_spirv),
                                            &layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 8;
        fill_array_sizes(layout);
        prog_pyramid_ = device.request_program(pyramid_down_spirv,
                                               sizeof(pyramid_down_spirv),
                                               &layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask =
            (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
        layout.sets[0].fp_mask =
            (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
        layout.push_constant_size = 20;
        fill_array_sizes(layout);
        prog_flow_ = device.request_program(flow_refine_spirv,
                                            sizeof(flow_refine_spirv), &layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 8;
        fill_array_sizes(layout);
        prog_smooth_ = device.request_program(flow_smooth_spirv,
                                              sizeof(flow_smooth_spirv),
                                              &layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].storage_buffer_mask = 1u << 2;
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 8;
        fill_array_sizes(layout);
        prog_scene_reduce_ = device.request_program(
            scene_reduce_spirv, sizeof(scene_reduce_spirv), &layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_buffer_mask = 1u << 0;
        layout.push_constant_size = 4;
        fill_array_sizes(layout);
        prog_scene_finalize_ = device.request_program(
            scene_finalize_spirv, sizeof(scene_finalize_spirv), &layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].sampled_image_mask =
            (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
        layout.sets[0].storage_image_mask = 1u << 4;
        layout.sets[0].storage_buffer_mask = 1u << 5;
        layout.sets[0].fp_mask =
            (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4);
        layout.push_constant_size = 24;
        fill_array_sizes(layout);
        prog_warp_ = device.request_program(warp_blend_spirv,
                                            sizeof(warp_blend_spirv), &layout);
    }

    programs_ready_ = prog_luma_ && prog_pyramid_ && prog_flow_ &&
                      prog_smooth_ && prog_scene_reduce_ &&
                      prog_scene_finalize_ && prog_warp_;
    if (!programs_ready_)
        Utils::warn("FrameInterpolator: failed to create compute programs");
#endif
}

void FrameInterpolator::ensure_resources(Vulkan::Device &device, unsigned w,
                                         unsigned h) {
    if (w == scan_w_ && h == scan_h_ && output_ && scene_buf_)
        return;

    scan_w_ = w;
    scan_h_ = h;

    unsigned lw = std::min(w, kTargetLumaWidth);
    unsigned lh = std::max(1u, (h * lw + w / 2) / std::max(w, 1u));
    num_levels_ = 0;
    while (num_levels_ < kMaxLevels) {
        luma_w_[num_levels_] = std::max(1u, lw);
        luma_h_[num_levels_] = std::max(1u, lh);
        ++num_levels_;
        if (lw <= 32 || lh <= 32)
            break;
        lw = (lw + 1) / 2;
        lh = (lh + 1) / 2;
    }

    for (unsigned i = 0; i < num_levels_; ++i) {
        luma_a_[i] = create_storage_image(device, luma_w_[i], luma_h_[i],
                                          VK_FORMAT_R16_SFLOAT);
        luma_b_[i] = create_storage_image(device, luma_w_[i], luma_h_[i],
                                          VK_FORMAT_R16_SFLOAT);
        flow_ab_[i] = create_storage_image(device, luma_w_[i], luma_h_[i],
                                           VK_FORMAT_R16G16_SFLOAT);
        flow_ba_[i] = create_storage_image(device, luma_w_[i], luma_h_[i],
                                           VK_FORMAT_R16G16_SFLOAT);
    }
    flow_tmp_ = create_storage_image(device, luma_w_[0], luma_h_[0],
                                     VK_FORMAT_R16G16_SFLOAT);
    output_ = create_storage_image(device, w, h, VK_FORMAT_R8G8B8A8_UNORM);

    Vulkan::BufferCreateInfo binfo;
    binfo.size = sizeof(uint32_t) * 4;
    binfo.usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    binfo.domain = Vulkan::BufferDomain::Device;
    binfo.misc = Vulkan::BUFFER_MISC_ZERO_INITIALIZE_BIT;
    scene_buf_ = device.create_buffer(binfo);
}

void FrameInterpolator::build_pyramid(Vulkan::CommandBuffer &cmd,
                                      Vulkan::Image &src,
                                      Vulkan::ImageHandle *levels) {
#if !N64_FRAME_INTERP
    (void)cmd;
    (void)src;
    (void)levels;
#else
    {
        struct Push {
            uint32_t w, h;
        } push{luma_w_[0], luma_h_[0]};
        cmd.set_program(prog_luma_);
        cmd.set_texture(0, 0, src.get_view(),
                        Vulkan::StockSampler::LinearClamp);
        cmd.set_storage_texture(0, 1, levels[0]->get_view());
        cmd.push_constants(&push, 0, sizeof(push));
        dispatch_2d(cmd, luma_w_[0], luma_h_[0]);
        storage_barrier(cmd, *levels[0]);
    }
    for (unsigned i = 1; i < num_levels_; ++i) {
        struct Push {
            uint32_t w, h;
        } push{luma_w_[i], luma_h_[i]};
        cmd.set_program(prog_pyramid_);
        cmd.set_storage_texture(0, 0, levels[i - 1]->get_view());
        cmd.set_storage_texture(0, 1, levels[i]->get_view());
        cmd.push_constants(&push, 0, sizeof(push));
        dispatch_2d(cmd, luma_w_[i], luma_h_[i]);
        storage_barrier(cmd, *levels[i]);
    }
#endif
}

void FrameInterpolator::estimate_flow(Vulkan::CommandBuffer &cmd) {
#if !N64_FRAME_INTERP
    (void)cmd;
#else
    auto run_dir = [&](bool reverse, Vulkan::ImageHandle *flow) {
        for (int level = int(num_levels_) - 1; level >= 0; --level) {
            const unsigned w = luma_w_[level];
            const unsigned h = luma_h_[level];
            const int has_prev = (level + 1 < int(num_levels_)) ? 1 : 0;
            const int radius = (level == int(num_levels_) - 1) ? 4 : 2;

            if (flow_tmp_->get_width() != w || flow_tmp_->get_height() != h) {
                flow_tmp_ = create_storage_image(cmd.get_device(), w, h,
                                                 VK_FORMAT_R16G16_SFLOAT);
            }

            Vulkan::Image *seed =
                has_prev ? flow[level + 1].get() : flow_tmp_.get();

            struct Push {
                uint32_t w, h;
                int32_t has_prev;
                int32_t radius;
                int32_t reverse;
            } push{w, h, has_prev, radius, reverse ? 1 : 0};

            cmd.set_program(prog_flow_);
            cmd.set_storage_texture(0, 0, luma_a_[level]->get_view());
            cmd.set_storage_texture(0, 1, luma_b_[level]->get_view());
            cmd.set_storage_texture(0, 2, seed->get_view());
            cmd.set_storage_texture(0, 3, flow[level]->get_view());
            cmd.push_constants(&push, 0, sizeof(push));
            dispatch_2d(cmd, w, h);
            storage_barrier(cmd, *flow[level]);

            struct SPush {
                uint32_t w, h;
            } spush{w, h};
            cmd.set_program(prog_smooth_);
            cmd.set_storage_texture(0, 0, flow[level]->get_view());
            cmd.set_storage_texture(0, 1, flow_tmp_->get_view());
            cmd.push_constants(&spush, 0, sizeof(spush));
            dispatch_2d(cmd, w, h);
            storage_barrier(cmd, *flow_tmp_);
            std::swap(flow[level], flow_tmp_);
        }
    };

    run_dir(false, flow_ab_);
    run_dir(true, flow_ba_);
#endif
}

void FrameInterpolator::reduce_scene(Vulkan::CommandBuffer &cmd) {
#if !N64_FRAME_INTERP
    (void)cmd;
#else
    cmd.fill_buffer(*scene_buf_, 0);
    cmd.buffer_barrier(*scene_buf_, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                       VK_ACCESS_2_TRANSFER_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                           VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

    const unsigned level = num_levels_ - 1;
    struct Push {
        uint32_t w, h;
    } push{luma_w_[level], luma_h_[level]};
    cmd.set_program(prog_scene_reduce_);
    cmd.set_storage_texture(0, 0, luma_a_[level]->get_view());
    cmd.set_storage_texture(0, 1, luma_b_[level]->get_view());
    cmd.set_storage_buffer(0, 2, *scene_buf_);
    cmd.push_constants(&push, 0, sizeof(push));
    dispatch_2d(cmd, luma_w_[level], luma_h_[level]);

    cmd.buffer_barrier(*scene_buf_, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                           VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

    float thr = kSceneThreshold;
    cmd.set_program(prog_scene_finalize_);
    cmd.set_storage_buffer(0, 0, *scene_buf_);
    cmd.push_constants(&thr, 0, sizeof(thr));
    cmd.dispatch(1, 1, 1);

    cmd.buffer_barrier(*scene_buf_, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
#endif
}

Vulkan::ImageHandle FrameInterpolator::warp(Vulkan::Device &device,
                                            float phase) {
#if !N64_FRAME_INTERP
    (void)device;
    (void)phase;
    return curr_novel_;
#else
    auto cmd = device.request_command_buffer();

    struct Push {
        uint32_t out_w, out_h;
        float flow_scale_x, flow_scale_y;
        float phase;
        float debug;
    } push;
    push.out_w = scan_w_;
    push.out_h = scan_h_;
    push.flow_scale_x = float(scan_w_) / float(std::max(1u, luma_w_[0]));
    push.flow_scale_y = float(scan_h_) / float(std::max(1u, luma_h_[0]));
    push.phase = phase;
    push.debug = debug_ ? 1.f : 0.f;

    cmd->set_program(prog_warp_);
    cmd->set_texture(0, 0, prev_novel_->get_view(),
                     Vulkan::StockSampler::LinearClamp);
    cmd->set_texture(0, 1, curr_novel_->get_view(),
                     Vulkan::StockSampler::LinearClamp);
    cmd->set_texture(0, 2, flow_ab_[0]->get_view(),
                     Vulkan::StockSampler::LinearClamp);
    cmd->set_texture(0, 3, flow_ba_[0]->get_view(),
                     Vulkan::StockSampler::LinearClamp);
    cmd->set_storage_texture(0, 4, output_->get_view());
    cmd->set_storage_buffer(0, 5, *scene_buf_);
    cmd->push_constants(&push, 0, sizeof(push));
    dispatch_2d(*cmd, scan_w_, scan_h_);
    storage_barrier(*cmd, *output_);

    device.submit(cmd);
    return output_;
#endif
}

Vulkan::ImageHandle FrameInterpolator::process(Vulkan::Device &device,
                                               Vulkan::ImageHandle scanout,
                                               uint32_t origin) {
#if !N64_FRAME_INTERP
    (void)device;
    (void)origin;
    return scanout;
#else
    if (!enabled_ || !scanout)
        return scanout;

    static const bool env_debug = [] {
        const char *e = getenv("N64_FRAME_INTERP_DEBUG");
        return e && e[0] != '\0' && e[0] != '0';
    }();
    debug_ = env_debug;

    ensure_programs(device);
    if (!programs_ready_)
        return scanout;

    ensure_resources(device, scanout->get_width(), scanout->get_height());

    const bool novel = !have_origin_ || origin != last_origin_;
    if (novel) {
        // If the previous transition still has queued presents, drop them to
        // avoid mixing flow contexts when cadence changes abruptly.
        if (!queue_.empty())
            queue_.clear();

        if (curr_novel_) {
            const unsigned k = std::clamp(hold_count_, 1u, kMaxHold);
            if (k == 1)
                ++consecutive_k1_;
            else
                consecutive_k1_ = 0;

            prev_novel_ = curr_novel_;
            curr_novel_ = scanout;

            if (consecutive_k1_ < kBypassStreak && k >= 2 && prev_novel_) {
                auto cmd = device.request_command_buffer();
                build_pyramid(*cmd, *prev_novel_, luma_a_);
                build_pyramid(*cmd, *curr_novel_, luma_b_);
                estimate_flow(*cmd);
                reduce_scene(*cmd);
                device.submit(cmd);

                for (unsigned i = 1; i < k; ++i) {
                    QueueItem item;
                    item.phase = float(i) / float(k);
                    queue_.push_back(item);
                }
                QueueItem end;
                end.phase = -1.f;
                end.frame = curr_novel_;
                queue_.push_back(end);
            } else {
                QueueItem end;
                end.phase = -1.f;
                end.frame = curr_novel_;
                queue_.push_back(end);
            }
        } else {
            curr_novel_ = scanout;
        }

        last_origin_ = origin;
        have_origin_ = true;
        hold_count_ = 1;
    } else {
        if (hold_count_ < kMaxHold)
            ++hold_count_;
    }

    if (consecutive_k1_ >= kBypassStreak) {
        queue_.clear();
        return scanout;
    }

    if (queue_.empty())
        return scanout;

    QueueItem item = queue_.front();
    queue_.pop_front();
    if (item.phase < 0.f)
        return item.frame ? item.frame : scanout;
    return warp(device, item.phase);
#endif
}

} // namespace PRDPWrapper
} // namespace N64

#include "video/frame_interpolate.h"
#include "utils/log.h"
#include "limits.hpp"

#ifndef N64_FRAME_INTERP
#define N64_FRAME_INTERP 0
#endif

#if N64_FRAME_INTERP
#include "content_compare_spirv.h"
#include "content_finalize_spirv.h"
#include "content_fingerprint_spirv.h"
#include "flow_advect_spirv.h"
#include "flow_consistency_spirv.h"
#include "flow_global_finalize_spirv.h"
#include "flow_global_reduce_spirv.h"
#include "flow_refine_spirv.h"
#include "flow_scale_spirv.h"
#include "flow_smooth_spirv.h"
#include "flow_temporal_blend_spirv.h"
#include "flow_temporal_save_spirv.h"
#include "luma_downsample_spirv.h"
#include "pyramid_down_spirv.h"
#include "scene_finalize_spirv.h"
#include "scene_reduce_spirv.h"
#include "warp_blend_spirv.h"
#endif

#include <algorithm>
#include <cstdlib>

namespace N64 {
namespace Video {
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

constexpr VkFormat kFlowFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

Vulkan::Program *req(Vulkan::Device &device, const uint32_t *spirv, size_t n,
                     Vulkan::ResourceLayout &layout) {
    fill_array_sizes(layout);
    return device.request_program(spirv, n, &layout);
}

// `sync` serializes against the GPU so the caller's wall clock measures GPU
// cost. Diagnostic only: it destroys pipelining.
void submit_maybe_sync(Vulkan::Device &device, Vulkan::CommandBufferHandle &cmd,
                       bool sync) {
    if (!sync) {
        device.submit(cmd);
        return;
    }
    Vulkan::Fence fence;
    device.submit(cmd, &fence);
    fence->wait();
}

} // namespace

void FrameInterpolator::reset() {
    have_origin_ = false;
    last_origin_ = 0;
    hold_count_ = 0;
    consecutive_k1_ = 0;
    pair_k_ = 1;
    prev_novel_.reset();
    curr_novel_.reset();
    prev_depth_.reset();
    curr_depth_.reset();
    depth_dummy_.reset();
    queue_.clear();
    scan_w_ = scan_h_ = 0;
    warp_w_ = warp_h_ = 0;
    num_levels_ = 0;
    for (unsigned i = 0; i < kMaxLevels; ++i) {
        luma_a_[i].reset();
        luma_b_[i].reset();
        flow_ab_[i].reset();
        flow_ba_[i].reset();
        luma_w_[i] = luma_h_[i] = 0;
    }
    flow_tmp_.reset();
    flow_seed_ab_.reset();
    flow_seed_ba_.reset();
    prev_flow_ab_.reset();
    prev_flow_ba_.reset();
    have_prev_flow_ = false;
    have_flow_ = false;
    output_.reset();
    output_hi_.reset();
    scene_buf_.reset();
    fp_prev_.reset();
    fp_curr_.reset();
    content_buf_.reset();
    content_readback_.reset();
    content_fence_.reset();
    have_fp_ = false;
    content_pending_ = false;
    content_changed_latched_ = true;
    global_buf_.reset();
    global_out_.reset();
    have_global_ = false;
    have_novel_t_ = false;
    stats_pairs_ = 0;
    stats_pair_ms_sum_ = 0.0;
    stats_k_sum_ = 0;
    timings_ = {};
}

FrameInterpolator::Timings FrameInterpolator::take_timings() {
    Timings t = timings_;
    timings_ = {};
    return t;
}

void FrameInterpolator::set_upscale(unsigned upscale) {
    upscale = std::max(1u, upscale);
    if (upscale == upscale_)
        return;
    upscale_ = upscale;
    // Force ensure_resources to rebuild warp/output sizes.
    scan_w_ = scan_h_ = 0;
    warp_w_ = warp_h_ = 0;
    output_.reset();
    output_hi_.reset();
}

void FrameInterpolator::clear_temporal_flow() {
    have_prev_flow_ = false;
    have_flow_ = false;
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
        prog_luma_ = req(device, luma_downsample_spirv,
                         sizeof(luma_downsample_spirv), layout);
        prog_fp_ = req(device, content_fingerprint_spirv,
                       sizeof(content_fingerprint_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 8;
        prog_pyramid_ =
            req(device, pyramid_down_spirv, sizeof(pyramid_down_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask =
            (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
        layout.sets[0].fp_mask =
            (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
        layout.push_constant_size = 24;
        prog_flow_ =
            req(device, flow_refine_spirv, sizeof(flow_refine_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 8;
        prog_smooth_ =
            req(device, flow_smooth_spirv, sizeof(flow_smooth_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 24; // dst, src, scale, pair_scale
        prog_advect_ =
            req(device, flow_advect_spirv, sizeof(flow_advect_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 16;
        prog_consistency_ = req(device, flow_consistency_spirv,
                                sizeof(flow_consistency_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 12;
        prog_temporal_blend_ = req(device, flow_temporal_blend_spirv,
                                   sizeof(flow_temporal_blend_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].storage_buffer_mask = 1u << 2;
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 8;
        prog_temporal_save_ = req(device, flow_temporal_save_spirv,
                                  sizeof(flow_temporal_save_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = 1u << 0;
        layout.sets[0].fp_mask = 1u << 0;
        layout.push_constant_size = 12; // uvec2 + scale
        prog_flow_scale_ =
            req(device, flow_scale_spirv, sizeof(flow_scale_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = (1u << 0) | (1u << 1);
        layout.sets[0].storage_buffer_mask = 1u << 2;
        layout.sets[0].fp_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 8;
        prog_scene_reduce_ =
            req(device, scene_reduce_spirv, sizeof(scene_reduce_spirv), layout);
        prog_content_cmp_ = req(device, content_compare_spirv,
                                sizeof(content_compare_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_buffer_mask = 1u << 0;
        layout.push_constant_size = 4;
        prog_scene_finalize_ = req(device, scene_finalize_spirv,
                                   sizeof(scene_finalize_spirv), layout);
        prog_content_fin_ = req(device, content_finalize_spirv,
                                sizeof(content_finalize_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_image_mask = 1u << 0;
        layout.sets[0].storage_buffer_mask = 1u << 1;
        layout.sets[0].fp_mask = 1u << 0;
        layout.push_constant_size = 12;
        prog_global_ = req(device, flow_global_reduce_spirv,
                           sizeof(flow_global_reduce_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].storage_buffer_mask = (1u << 0) | (1u << 1);
        layout.push_constant_size = 8; // vec2 flow_scale
        prog_global_fin_ = req(device, flow_global_finalize_spirv,
                               sizeof(flow_global_finalize_spirv), layout);
    }
    {
        Vulkan::ResourceLayout layout = {};
        layout.sets[0].sampled_image_mask =
            (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 6) |
            (1u << 7) | (1u << 8);
        layout.sets[0].storage_image_mask = 1u << 4;
        layout.sets[0].storage_buffer_mask = (1u << 5) | (1u << 9);
        layout.sets[0].fp_mask =
            (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) |
            (1u << 6) | (1u << 7) | (1u << 8);
        layout.push_constant_size = 48; // have_global instead of vec2 globals
        prog_warp_ =
            req(device, warp_blend_spirv, sizeof(warp_blend_spirv), layout);
    }

    programs_ready_ =
        prog_luma_ && prog_pyramid_ && prog_flow_ && prog_smooth_ &&
        prog_advect_ && prog_consistency_ && prog_temporal_blend_ &&
        prog_temporal_save_ && prog_flow_scale_ && prog_scene_reduce_ &&
        prog_scene_finalize_ && prog_warp_ && prog_fp_ && prog_content_cmp_ &&
        prog_content_fin_ && prog_global_ && prog_global_fin_;
    if (!programs_ready_)
        Utils::warn("FrameInterpolator: failed to create compute programs");
#endif
}

void FrameInterpolator::ensure_resources(Vulkan::Device &device, unsigned w,
                                         unsigned h) {
    if (w == scan_w_ && h == scan_h_ && output_ && scene_buf_ && fp_curr_)
        return;

    clear_temporal_flow();
    have_fp_ = false;

    scan_w_ = w;
    scan_h_ = h;
    // Warp at native (pre-RDP-upscale) resolution. Motion vectors are estimated
    // on a <=320 luma pyramid either way; writing intermediates at 4x only burns
    // bandwidth.
    warp_w_ = std::max(1u, w / std::max(1u, upscale_));
    warp_h_ = std::max(1u, h / std::max(1u, upscale_));

    unsigned lw = std::min(warp_w_, kTargetLumaWidth);
    unsigned lh = std::max(1u, (warp_h_ * lw + warp_w_ / 2) / std::max(warp_w_, 1u));
    num_levels_ = 0;
    while (num_levels_ < kMaxLevels) {
        luma_w_[num_levels_] = std::max(1u, lw);
        luma_h_[num_levels_] = std::max(1u, lh);
        ++num_levels_;
        if (lw <= kCoarsestSize || lh <= kCoarsestSize)
            break;
        lw = (lw + 1) / 2;
        lh = (lh + 1) / 2;
    }

    for (unsigned i = 0; i < num_levels_; ++i) {
        luma_a_[i] = create_storage_image(device, luma_w_[i], luma_h_[i],
                                          VK_FORMAT_R16_SFLOAT);
        luma_b_[i] = create_storage_image(device, luma_w_[i], luma_h_[i],
                                          VK_FORMAT_R16_SFLOAT);
        flow_ab_[i] =
            create_storage_image(device, luma_w_[i], luma_h_[i], kFlowFormat);
        flow_ba_[i] =
            create_storage_image(device, luma_w_[i], luma_h_[i], kFlowFormat);
    }
    flow_tmp_ =
        create_storage_image(device, luma_w_[0], luma_h_[0], kFlowFormat);
    const unsigned coarse = num_levels_ - 1;
    flow_seed_ab_ = create_storage_image(device, luma_w_[coarse],
                                         luma_h_[coarse], kFlowFormat);
    flow_seed_ba_ = create_storage_image(device, luma_w_[coarse],
                                         luma_h_[coarse], kFlowFormat);
    prev_flow_ab_ =
        create_storage_image(device, luma_w_[0], luma_h_[0], kFlowFormat);
    prev_flow_ba_ =
        create_storage_image(device, luma_w_[0], luma_h_[0], kFlowFormat);
    output_ =
        create_storage_image(device, warp_w_, warp_h_, VK_FORMAT_R8G8B8A8_UNORM);
    if (upscale_ > 1)
        output_hi_ =
            create_storage_image(device, w, h, VK_FORMAT_R8G8B8A8_UNORM);
    else
        output_hi_.reset();

    fp_prev_ = create_storage_image(device, kFingerprintSize, kFingerprintSize,
                                    VK_FORMAT_R16_SFLOAT);
    fp_curr_ = create_storage_image(device, kFingerprintSize, kFingerprintSize,
                                    VK_FORMAT_R16_SFLOAT);

    auto make_ssbo = [&](Vulkan::BufferDomain domain, VkDeviceSize size) {
        Vulkan::BufferCreateInfo binfo;
        binfo.size = size;
        binfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        binfo.domain = domain;
        binfo.misc = Vulkan::BUFFER_MISC_ZERO_INITIALIZE_BIT;
        return device.create_buffer(binfo);
    };
    scene_buf_ = make_ssbo(Vulkan::BufferDomain::Device, sizeof(uint32_t) * 4);
    content_buf_ = make_ssbo(Vulkan::BufferDomain::Device, sizeof(uint32_t) * 4);
    content_readback_ =
        make_ssbo(Vulkan::BufferDomain::CachedHost, sizeof(uint32_t) * 4);
    global_buf_ = make_ssbo(Vulkan::BufferDomain::Device, sizeof(int32_t) * 8);
    global_out_ = make_ssbo(Vulkan::BufferDomain::Device, sizeof(float) * 4);
    content_fence_.reset();
    content_pending_ = false;
    have_global_ = false;
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
    const unsigned coarse = num_levels_ - 1;
    const bool use_temporal = have_prev_flow_ && flag_temporal_;

    if (use_temporal) {
        const float scale =
            float(luma_w_[coarse]) / float(std::max(1u, luma_w_[0]));
        struct APush {
            uint32_t dst_w, dst_h;
            uint32_t src_w, src_h;
            float scale;
            float pair_scale;
        } apush{luma_w_[coarse], luma_h_[coarse], luma_w_[0], luma_h_[0], scale,
                float(std::max(1u, pair_k_))};

        auto advect = [&](Vulkan::Image &src, Vulkan::Image &dst) {
            cmd.set_program(prog_advect_);
            cmd.set_storage_texture(0, 0, src.get_view());
            cmd.set_storage_texture(0, 1, dst.get_view());
            cmd.push_constants(&apush, 0, sizeof(apush));
            dispatch_2d(cmd, luma_w_[coarse], luma_h_[coarse]);
            storage_barrier(cmd, dst);
        };
        advect(*prev_flow_ab_, *flow_seed_ab_);
        advect(*prev_flow_ba_, *flow_seed_ba_);
    }

    auto run_dir = [&](bool reverse, Vulkan::ImageHandle *flow,
                       Vulkan::ImageHandle &seed) {
        for (int level = int(num_levels_) - 1; level >= 0; --level) {
            const unsigned w = luma_w_[level];
            const unsigned h = luma_h_[level];
            const bool is_coarse = (level == int(coarse));
            const bool has_pyramid_prev = (level + 1 < int(num_levels_));
            int has_prev = 0;
            if (has_pyramid_prev)
                has_prev = 1;
            else if (is_coarse && use_temporal)
                has_prev = 2;
            // Coarse: r=2; fine: r=1 (was 4/2  Edominant cost).
            int radius = is_coarse ? 2 : 1;

            if (flow_tmp_->get_width() != w || flow_tmp_->get_height() != h) {
                flow_tmp_ =
                    create_storage_image(cmd.get_device(), w, h, kFlowFormat);
            }

            Vulkan::Image *seed_img = flow_tmp_.get();
            if (has_pyramid_prev)
                seed_img = flow[level + 1].get();
            else if (is_coarse && use_temporal)
                seed_img = seed.get();

            struct Push {
                uint32_t w, h;
                int32_t has_prev;
                int32_t radius;
                int32_t reverse;
                int32_t enable_subpixel;
            } push{w,
                   h,
                   has_prev,
                   radius,
                   reverse ? 1 : 0,
                   flag_subpixel_ ? 1 : 0};

            cmd.set_program(prog_flow_);
            cmd.set_storage_texture(0, 0, luma_a_[level]->get_view());
            cmd.set_storage_texture(0, 1, luma_b_[level]->get_view());
            cmd.set_storage_texture(0, 2, seed_img->get_view());
            cmd.set_storage_texture(0, 3, flow[level]->get_view());
            cmd.push_constants(&push, 0, sizeof(push));
            dispatch_2d(cmd, w, h);
            storage_barrier(cmd, *flow[level]);

            // Skip median smooth on finest level (most of the smooth cost).
            if (flag_flow_smooth_ && level > 0) {
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
        }
    };

    run_dir(false, flow_ab_, flow_seed_ab_);
    run_dir(true, flow_ba_, flow_seed_ba_);
#endif
}

void FrameInterpolator::apply_consistency(Vulkan::CommandBuffer &cmd) {
#if !N64_FRAME_INTERP
    (void)cmd;
#else
    struct Push {
        uint32_t w, h;
        float alpha;
        float beta;
    } push{luma_w_[0], luma_h_[0], kConsistencyAlpha, kConsistencyBeta};
    cmd.set_program(prog_consistency_);
    cmd.set_storage_texture(0, 0, flow_ab_[0]->get_view());
    cmd.set_storage_texture(0, 1, flow_ba_[0]->get_view());
    cmd.push_constants(&push, 0, sizeof(push));
    dispatch_2d(cmd, luma_w_[0], luma_h_[0]);
    storage_barrier(cmd, *flow_ab_[0]);
    storage_barrier(cmd, *flow_ba_[0]);
#endif
}

void FrameInterpolator::normalize_and_blend_temporal(Vulkan::CommandBuffer &cmd,
                                                     unsigned k) {
#if !N64_FRAME_INTERP
    (void)cmd;
    (void)k;
#else
    const float inv_k = 1.f / float(std::max(1u, k));
    auto scale = [&](Vulkan::Image &img, float s) {
        struct Push {
            uint32_t w, h;
            float scale;
        } push{luma_w_[0], luma_h_[0], s};
        cmd.set_program(prog_flow_scale_);
        cmd.set_storage_texture(0, 0, img.get_view());
        cmd.push_constants(&push, 0, sizeof(push));
        dispatch_2d(cmd, luma_w_[0], luma_h_[0]);
        storage_barrier(cmd, img);
    };

    // Displacement ↁEper-field velocity for EMA / store.
    scale(*flow_ab_[0], inv_k);
    scale(*flow_ba_[0], inv_k);

    if (have_prev_flow_ && flag_temporal_smooth_) {
        struct Push {
            uint32_t w, h;
            float alpha;
        } push{luma_w_[0], luma_h_[0], kTemporalFlowAlpha};
        auto blend_one = [&](Vulkan::Image &prev, Vulkan::Image &cur) {
            cmd.set_program(prog_temporal_blend_);
            cmd.set_storage_texture(0, 0, prev.get_view());
            cmd.set_storage_texture(0, 1, cur.get_view());
            cmd.push_constants(&push, 0, sizeof(push));
            dispatch_2d(cmd, luma_w_[0], luma_h_[0]);
            storage_barrier(cmd, cur);
        };
        blend_one(*prev_flow_ab_, *flow_ab_[0]);
        blend_one(*prev_flow_ba_, *flow_ba_[0]);
    }

    // Restore displacement for warping (velocity * k).
    scale(*flow_ab_[0], float(std::max(1u, k)));
    scale(*flow_ba_[0], float(std::max(1u, k)));
#endif
}

void FrameInterpolator::save_temporal_velocity(Vulkan::CommandBuffer &cmd,
                                               unsigned k) {
#if !N64_FRAME_INTERP
    (void)cmd;
    (void)k;
#else
    // flow_* currently holds displacement; convert to velocity while saving.
    const float inv_k = 1.f / float(std::max(1u, k));
    auto scale = [&](Vulkan::Image &img, float s) {
        struct Push {
            uint32_t w, h;
            float scale;
        } push{luma_w_[0], luma_h_[0], s};
        cmd.set_program(prog_flow_scale_);
        cmd.set_storage_texture(0, 0, img.get_view());
        cmd.push_constants(&push, 0, sizeof(push));
        dispatch_2d(cmd, luma_w_[0], luma_h_[0]);
        storage_barrier(cmd, img);
    };
    scale(*flow_ab_[0], inv_k);
    scale(*flow_ba_[0], inv_k);

    struct Push {
        uint32_t w, h;
    } push{luma_w_[0], luma_h_[0]};
    auto save_one = [&](Vulkan::Image &src, Vulkan::Image &dst) {
        cmd.set_program(prog_temporal_save_);
        cmd.set_storage_texture(0, 0, src.get_view());
        cmd.set_storage_texture(0, 1, dst.get_view());
        cmd.set_storage_buffer(0, 2, *scene_buf_);
        cmd.push_constants(&push, 0, sizeof(push));
        dispatch_2d(cmd, luma_w_[0], luma_h_[0]);
        storage_barrier(cmd, dst);
    };
    save_one(*flow_ab_[0], *prev_flow_ab_);
    save_one(*flow_ba_[0], *prev_flow_ba_);

    // Restore displacement for any subsequent warps this pair.
    scale(*flow_ab_[0], float(std::max(1u, k)));
    scale(*flow_ba_[0], float(std::max(1u, k)));

    have_prev_flow_ = true;
#endif
}

void FrameInterpolator::append_global_motion(Vulkan::CommandBuffer &cmd) {
#if !N64_FRAME_INTERP
    (void)cmd;
#else
    if (!flag_global_motion_) {
        have_global_ = false;
        return;
    }

    cmd.fill_buffer(*global_buf_, 0);
    cmd.buffer_barrier(*global_buf_, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                       VK_ACCESS_2_TRANSFER_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                           VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

    struct Push {
        uint32_t w, h;
        float conf_min;
    } push{luma_w_[0], luma_h_[0], kGlobalConfMin};

    auto reduce_at = [&](Vulkan::Image &flow, VkDeviceSize offset) {
        cmd.set_program(prog_global_);
        cmd.set_storage_texture(0, 0, flow.get_view());
        cmd.set_storage_buffer(0, 1, *global_buf_, offset, sizeof(int32_t) * 4);
        cmd.push_constants(&push, 0, sizeof(push));
        dispatch_2d(cmd, luma_w_[0], luma_h_[0]);
    };
    reduce_at(*flow_ab_[0], 0);
    reduce_at(*flow_ba_[0], sizeof(int32_t) * 4);

    cmd.buffer_barrier(*global_buf_, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

    struct FPush {
        float sx, sy;
    } fpush{float(scan_w_) / float(std::max(1u, luma_w_[0])),
            float(scan_h_) / float(std::max(1u, luma_h_[0]))};
    cmd.set_program(prog_global_fin_);
    cmd.set_storage_buffer(0, 0, *global_buf_);
    cmd.set_storage_buffer(0, 1, *global_out_);
    cmd.push_constants(&fpush, 0, sizeof(fpush));
    cmd.dispatch(1, 1, 1);
    cmd.buffer_barrier(*global_out_, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    have_global_ = true;
#endif
}

void FrameInterpolator::kick_content_fingerprint(Vulkan::Device &device,
                                                 Vulkan::Image &scanout,
                                                 bool do_compare) {
#if !N64_FRAME_INTERP
    (void)device;
    (void)scanout;
    (void)do_compare;
#else
    auto cmd = device.request_command_buffer();
    struct Push {
        uint32_t w, h;
    } fpush{kFingerprintSize, kFingerprintSize};
    cmd->set_program(prog_fp_);
    cmd->set_texture(0, 0, scanout.get_view(),
                     Vulkan::StockSampler::LinearClamp);
    cmd->set_storage_texture(0, 1, fp_curr_->get_view());
    cmd->push_constants(&fpush, 0, sizeof(fpush));
    dispatch_2d(*cmd, kFingerprintSize, kFingerprintSize);
    storage_barrier(*cmd, *fp_curr_);

    if (do_compare && have_fp_) {
        cmd->fill_buffer(*content_buf_, 0);
        cmd->buffer_barrier(*content_buf_, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                                VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
        struct RPush {
            uint32_t w, h;
        } rpush{kFingerprintSize, kFingerprintSize};
        cmd->set_program(prog_content_cmp_);
        cmd->set_storage_texture(0, 0, fp_prev_->get_view());
        cmd->set_storage_texture(0, 1, fp_curr_->get_view());
        cmd->set_storage_buffer(0, 2, *content_buf_);
        cmd->push_constants(&rpush, 0, sizeof(rpush));
        dispatch_2d(*cmd, kFingerprintSize, kFingerprintSize);

        cmd->buffer_barrier(*content_buf_, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        float thr = kContentThreshold;
        cmd->set_program(prog_content_fin_);
        cmd->set_storage_buffer(0, 0, *content_buf_);
        cmd->push_constants(&thr, 0, sizeof(thr));
        cmd->dispatch(1, 1, 1);
        cmd->copy_buffer(*content_readback_, *content_buf_);
        cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    }

    device.submit(cmd, &content_fence_);
    content_pending_ = do_compare && have_fp_;
    std::swap(fp_prev_, fp_curr_);
    have_fp_ = true;
#endif
}

bool FrameInterpolator::poll_content_novel(Vulkan::Device &device) {
#if !N64_FRAME_INTERP
    (void)device;
    return true;
#else
    if (!content_pending_ || !content_fence_)
        return content_changed_latched_;

    const auto fence_t0 = std::chrono::steady_clock::now();
    if (!content_fence_->wait_timeout(0)) {
        timings_.fence_wait_ms +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - fence_t0)
                .count();
        return content_changed_latched_;
    }
    timings_.fence_wait_ms +=
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - fence_t0)
            .count();
    content_pending_ = false;

    auto *raw = static_cast<const uint32_t *>(device.map_host_buffer(
        *content_readback_, Vulkan::MEMORY_ACCESS_READ_BIT));
    content_changed_latched_ = raw[2] != 0u;
    device.unmap_host_buffer(*content_readback_, Vulkan::MEMORY_ACCESS_READ_BIT);
    return content_changed_latched_;
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

void FrameInterpolator::note_latency_stats(unsigned k) {
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    double pair_ms = 0.0;
    if (have_novel_t_) {
        pair_ms = std::chrono::duration<double, std::milli>(now - last_novel_t_)
                      .count();
        stats_pair_ms_sum_ += pair_ms;
        stats_k_sum_ += k;
        ++stats_pairs_;
    }
    last_novel_t_ = now;
    have_novel_t_ = true;

    if (!stats_)
        return;

    if (stats_window_t_.time_since_epoch().count() == 0)
        stats_window_t_ = now;

    // Aggregate only  Eper-pair logging stalls the present thread on Windows consoles.
    const double window_ms =
        std::chrono::duration<double, std::milli>(now - stats_window_t_).count();
    if (window_ms >= 1000.0 && stats_pairs_ > 0) {
        Utils::info(
            "interp latency avg (1s): k={:.2f} pair={:.1f}ms over {} pairs "
            "(~1 source-frame added input delay)",
            double(stats_k_sum_) / double(stats_pairs_),
            stats_pair_ms_sum_ / double(stats_pairs_), stats_pairs_);
        stats_pairs_ = 0;
        stats_pair_ms_sum_ = 0.0;
        stats_k_sum_ = 0;
        stats_window_t_ = now;
    }
}

Vulkan::ImageHandle FrameInterpolator::warp(Vulkan::Device &device,
                                            float phase, bool extrapolate) {
#if !N64_FRAME_INTERP
    (void)device;
    (void)phase;
    (void)extrapolate;
    return curr_novel_;
#else
    const auto warp_t0 = std::chrono::steady_clock::now();
    auto cmd = device.request_command_buffer();

    if (!depth_dummy_) {
        depth_dummy_ = create_storage_image(device, 1, 1, VK_FORMAT_R16_SFLOAT);
        auto clear_cmd = device.request_command_buffer();
        VkClearValue cv{};
        cv.color.float32[0] = 1.f;
        clear_cmd->clear_image(*depth_dummy_, cv);
        device.submit(clear_cmd);
    }

    const bool use_depth = flag_depth_occl_ && prev_depth_ && curr_depth_;
    Vulkan::Image &depth_a = use_depth ? *prev_depth_ : *depth_dummy_;
    Vulkan::Image &depth_b = use_depth ? *curr_depth_ : *depth_dummy_;

    struct Push {
        uint32_t out_w, out_h;
        float flow_scale_x, flow_scale_y;
        float phase;
        float debug;
        float onesided;
        float static_snap;
        float obmc;
        float have_prev;
        float blend_fallback;
        float have_depth;
        float have_global;
        float extrapolate;
    } push{};
    push.out_w = warp_w_;
    push.out_h = warp_h_;
    push.flow_scale_x = float(warp_w_) / float(std::max(1u, luma_w_[0]));
    push.flow_scale_y = float(warp_h_) / float(std::max(1u, luma_h_[0]));
    push.phase = phase;
    push.debug = debug_ ? 1.f : 0.f;
    push.onesided = flag_onesided_ ? 1.f : 0.f;
    push.static_snap = flag_static_snap_ ? 1.f : 0.f;
    push.obmc = flag_obmc_ ? 1.f : 0.f;
    push.have_prev = (have_prev_flow_ && prev_flow_ab_) ? 1.f : 0.f;
    push.blend_fallback = flag_blend_fallback_ ? 1.f : 0.f;
    push.have_depth = use_depth ? 1.f : 0.f;
    push.have_global = (flag_global_motion_ && have_global_) ? 1.f : 0.f;
    push.extrapolate = extrapolate ? 1.f : 0.f;

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
    cmd->set_texture(0, 6,
                     (prev_flow_ab_ ? prev_flow_ab_ : flow_ab_[0])->get_view(),
                     Vulkan::StockSampler::LinearClamp);
    cmd->set_texture(0, 7, depth_a.get_view(),
                     Vulkan::StockSampler::LinearClamp);
    cmd->set_texture(0, 8, depth_b.get_view(),
                     Vulkan::StockSampler::LinearClamp);
    cmd->set_storage_buffer(0, 9, *global_out_);
    cmd->push_constants(&push, 0, sizeof(push));
    dispatch_2d(*cmd, warp_w_, warp_h_);
    storage_barrier(*cmd, *output_);

    Vulkan::ImageHandle present = output_;
    if (upscale_ > 1 && output_hi_) {
        // Restore scanout resolution so novel (4x) and intermediate match.
        cmd->image_barrier(
            *output_, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        cmd->image_barrier(
            *output_hi_, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        cmd->blit_image(
            *output_hi_, *output_, {},
            {int(scan_w_), int(scan_h_), 1}, {},
            {int(warp_w_), int(warp_h_), 1}, 0, 0, 0, 0, 1,
            VK_FILTER_LINEAR);
        cmd->image_barrier(
            *output_hi_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        cmd->image_barrier(
            *output_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        present = output_hi_;
    }

    submit_maybe_sync(device, cmd, profile_gpu_);
    timings_.warp_ms += std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - warp_t0)
                            .count();
    ++timings_.warps;
    return present;
#endif
}

bool FrameInterpolator::build_pair_flow(Vulkan::Device &device, unsigned k) {
#if !N64_FRAME_INTERP
    (void)device;
    (void)k;
    return false;
#else
    if (!prev_novel_ || !curr_novel_ || k < 2)
        return false;
    const auto t0 = std::chrono::steady_clock::now();
    auto cmd = device.request_command_buffer();
    build_pyramid(*cmd, *prev_novel_, luma_a_);
    build_pyramid(*cmd, *curr_novel_, luma_b_);
    reduce_scene(*cmd);
    estimate_flow(*cmd);
    apply_consistency(*cmd);
    normalize_and_blend_temporal(*cmd, k);
    save_temporal_velocity(*cmd, k);
    append_global_motion(*cmd);
    submit_maybe_sync(device, cmd, profile_gpu_);
    timings_.flow_ms += std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
    ++timings_.flows;
    have_flow_ = true;
    return true;
#endif
}

Vulkan::ImageHandle FrameInterpolator::process(Vulkan::Device &device,
                                               Vulkan::ImageHandle scanout,
                                               uint32_t origin,
                                               Vulkan::ImageHandle depth) {
#if !N64_FRAME_INTERP
    (void)device;
    (void)origin;
    (void)depth;
    return scanout;
#else
    if (!enabled_ || !scanout)
        return scanout;

    static const bool env_debug = [] {
        const char *e = getenv("N64_FRAME_INTERP_DEBUG");
        return e && e[0] != '\0' && e[0] != '0';
    }();
    static const bool env_stats = [] {
        const char *e = getenv("N64_FRAME_INTERP_STATS");
        return e && e[0] != '\0' && e[0] != '0';
    }();
    static const bool env_profile_gpu = [] {
        const char *e = getenv("N64_PROFILE_INTERP_GPU");
        return e && e[0] != '\0' && e[0] != '0';
    }();
    debug_ = env_debug;
    stats_ = env_stats;
    profile_gpu_ = env_profile_gpu;

    auto env_flag = [](const char *name, bool default_on) {
        const char *e = getenv(name);
        if (!e || e[0] == '\0')
            return default_on;
        return e[0] != '0';
    };
    flag_subpixel_ = env_flag("N64_FLOW_SUBPIXEL", false);
    flag_onesided_ = env_flag("N64_FLOW_ONESIDED", false);
    flag_temporal_ = env_flag("N64_FLOW_TEMPORAL", true);
    flag_static_snap_ = env_flag("N64_FLOW_STATIC_SNAP", true);
    flag_obmc_ = env_flag("N64_FLOW_OBMC", false);
    flag_blend_fallback_ = env_flag("N64_FLOW_BLEND_FALLBACK", false);
    flag_temporal_smooth_ = env_flag("N64_FLOW_TEMPORAL_SMOOTH", false);
    flag_depth_occl_ = env_flag("N64_FLOW_DEPTH_OCCL", true);
    flag_content_hash_ = env_flag("N64_FLOW_CONTENT_HASH", true);
    flag_global_motion_ = env_flag("N64_FLOW_GLOBAL_MOTION", false);
    flag_flow_smooth_ = env_flag("N64_FLOW_SMOOTH", true);

    // Env override for quick A/B of modes without rebuilding settings.
    if (const char *e = getenv("N64_FRAME_INTERP_MODE")) {
        if (e[0] == 'e' || e[0] == 'E' || e[0] == '1')
            mode_ = FrameInterpMode::Extrapolate;
        else if (e[0] == 'b' || e[0] == 'B' || e[0] == '0')
            mode_ = FrameInterpMode::Bidirectional;
    }

    ensure_programs(device);
    if (!programs_ready_)
        return scanout;

    ensure_resources(device, scanout->get_width(), scanout->get_height());

    const bool origin_changed = !have_origin_ || origin != last_origin_;
    bool novel = origin_changed;
    if (flag_content_hash_) {
        const bool content_novel = poll_content_novel(device);
        novel = origin_changed || content_novel;
        kick_content_fingerprint(device, *scanout, !origin_changed);
    }

    const bool extrapolate_mode = mode_ == FrameInterpMode::Extrapolate;

    if (novel) {
        if (!queue_.empty())
            queue_.clear();

        if (curr_novel_) {
            const unsigned k = std::clamp(hold_count_, 1u, kMaxHold);
            pair_k_ = k;
            note_latency_stats(k);

            if (k == 1)
                ++consecutive_k1_;
            else
                consecutive_k1_ = 0;

            prev_novel_ = curr_novel_;
            curr_novel_ = scanout;
            prev_depth_ = curr_depth_;
            curr_depth_ = depth;

            const bool can_flow =
                consecutive_k1_ < kBypassStreak && k >= 2 && prev_novel_;
            if (can_flow && build_pair_flow(device, k)) {
                if (!extrapolate_mode) {
                    for (unsigned i = 1; i < k; ++i) {
                        QueueItem item;
                        item.phase = float(i) / float(k);
                        queue_.push_back(item);
                    }
                    QueueItem end;
                    end.phase = -1.f;
                    end.frame = curr_novel_;
                    queue_.push_back(end);
                }
            } else {
                clear_temporal_flow();
                have_global_ = false;
                if (!extrapolate_mode) {
                    QueueItem end;
                    end.phase = -1.f;
                    end.frame = curr_novel_;
                    queue_.push_back(end);
                }
            }
        } else {
            curr_novel_ = scanout;
            curr_depth_ = depth;
            note_latency_stats(1);
        }

        last_origin_ = origin;
        have_origin_ = true;
        hold_count_ = 1;

        if (extrapolate_mode)
            return scanout; // present novel immediately (no +1 frame delay)
    } else {
        if (hold_count_ < kMaxHold)
            ++hold_count_;
    }

    if (consecutive_k1_ >= kBypassStreak) {
        queue_.clear();
        clear_temporal_flow();
        return scanout;
    }

    if (extrapolate_mode) {
        // Experimental: present novels immediately. Only a single small nudge
        // on the first duplicate field, then freeze — ramping extrapolation
        // every hold chatters, and overshoot snaps when the next novel arrives.
        if (!have_flow_ || !curr_novel_ || !prev_novel_ || hold_count_ != 2)
            return hold_count_ > 1 && curr_novel_ ? curr_novel_ : scanout;
        return warp(device, 0.12f, true);
    }

    if (queue_.empty())
        return scanout;

    QueueItem item = queue_.front();
    queue_.pop_front();
    if (item.phase < 0.f)
        return item.frame ? item.frame : scanout;
    return warp(device, item.phase, false);
#endif
}

} // namespace Video
} // namespace N64

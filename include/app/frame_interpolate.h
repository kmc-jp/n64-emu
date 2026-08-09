#pragma once

#include "device.hpp"
#include "image.hpp"
#include <cstdint>
#include <deque>

namespace N64 {
namespace PRDPWrapper {

// Optical-flow frame interpolator for VI duplicate-field replacement.
// When enabled, delays output by one game frame and fills held fields with
// warped intermediates between consecutive novel frames (VI_ORIGIN changes).
class FrameInterpolator {
  public:
    FrameInterpolator() = default;
    ~FrameInterpolator() = default;

    FrameInterpolator(const FrameInterpolator &) = delete;
    FrameInterpolator &operator=(const FrameInterpolator &) = delete;

    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }

    // Process one VI-field scanout. Returns the image to present (may be a
    // warped intermediate, a delayed novel frame, or the input passthrough).
    // `origin` is VI_ORIGIN (framebuffer pointer); used as the novel-frame signal.
    Vulkan::ImageHandle process(Vulkan::Device &device,
                                Vulkan::ImageHandle scanout, uint32_t origin);

    void reset();

  private:
    static constexpr unsigned kMaxLevels = 5;
    static constexpr unsigned kTargetLumaWidth = 512;
    static constexpr unsigned kBypassStreak = 4;
    static constexpr unsigned kMaxHold = 8;
    static constexpr float kSceneThreshold = 0.18f;

    struct QueueItem {
        // phase < 0 => present `frame` as-is; else warp between pair with phase.
        float phase = -1.f;
        Vulkan::ImageHandle frame; // used when phase < 0
    };

    bool enabled_ = false;
    bool debug_ = false;

    uint32_t last_origin_ = 0;
    bool have_origin_ = false;
    unsigned hold_count_ = 0;
    unsigned consecutive_k1_ = 0;

    Vulkan::ImageHandle prev_novel_;
    Vulkan::ImageHandle curr_novel_;

    unsigned scan_w_ = 0;
    unsigned scan_h_ = 0;
    unsigned luma_w_[kMaxLevels] = {};
    unsigned luma_h_[kMaxLevels] = {};
    unsigned num_levels_ = 0;

    // Double-buffered luma pyramids for prev/curr.
    Vulkan::ImageHandle luma_a_[kMaxLevels];
    Vulkan::ImageHandle luma_b_[kMaxLevels];
    Vulkan::ImageHandle flow_ab_[kMaxLevels];
    Vulkan::ImageHandle flow_ba_[kMaxLevels];
    Vulkan::ImageHandle flow_tmp_;
    Vulkan::ImageHandle output_;
    Vulkan::BufferHandle scene_buf_;

    Vulkan::Program *prog_luma_ = nullptr;
    Vulkan::Program *prog_pyramid_ = nullptr;
    Vulkan::Program *prog_flow_ = nullptr;
    Vulkan::Program *prog_smooth_ = nullptr;
    Vulkan::Program *prog_scene_reduce_ = nullptr;
    Vulkan::Program *prog_scene_finalize_ = nullptr;
    Vulkan::Program *prog_warp_ = nullptr;
    bool programs_ready_ = false;

    std::deque<QueueItem> queue_;

    void ensure_programs(Vulkan::Device &device);
    void ensure_resources(Vulkan::Device &device, unsigned w, unsigned h);
    void build_pyramid(Vulkan::CommandBuffer &cmd, Vulkan::Image &src,
                       Vulkan::ImageHandle *levels);
    void estimate_flow(Vulkan::CommandBuffer &cmd);
    void reduce_scene(Vulkan::CommandBuffer &cmd);
    Vulkan::ImageHandle warp(Vulkan::Device &device, float phase);
    static void dispatch_2d(Vulkan::CommandBuffer &cmd, unsigned w,
                            unsigned h);
};

} // namespace PRDPWrapper
} // namespace N64

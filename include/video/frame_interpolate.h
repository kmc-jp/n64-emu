#pragma once

#include "device.hpp"
#include "image.hpp"
#include <chrono>
#include <cstdint>
#include <deque>

namespace N64 {
namespace PRDPWrapper {

// Optical-flow frame interpolator for VI duplicate-field replacement.
// When enabled, delays output by one game frame and fills held fields with
// warped intermediates between consecutive novel frames.
// Novel frames: content fingerprint (primary) with VI_ORIGIN as cold-start aid.
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
    // `origin` is VI_ORIGIN (framebuffer pointer).
    // `depth` is an optional R16F linear Z snapshot for this origin (may be null).
    Vulkan::ImageHandle process(Vulkan::Device &device,
                                Vulkan::ImageHandle scanout, uint32_t origin,
                                Vulkan::ImageHandle depth = {});

    void reset();

  private:
    static constexpr unsigned kMaxLevels = 5;
    // Cap matching resolution  E512 was quality-heavy for realtime.
    static constexpr unsigned kTargetLumaWidth = 320;
    static constexpr unsigned kCoarsestSize = 20;
    static constexpr unsigned kFingerprintSize = 16;
    static constexpr unsigned kBypassStreak = 4;
    static constexpr unsigned kMaxHold = 8;
    static constexpr float kSceneThreshold = 0.18f;
    static constexpr float kContentThreshold = 0.02f;
    static constexpr float kConsistencyAlpha = 0.01f;
    static constexpr float kConsistencyBeta = 0.5f;
    static constexpr float kTemporalFlowAlpha = 0.5f;
    static constexpr float kGlobalConfMin = 0.25f;
    static constexpr float kStopVelEps = 0.03f;

    struct QueueItem {
        float phase = -1.f;
        Vulkan::ImageHandle frame;
    };

    bool enabled_ = false;
    bool debug_ = false;
    bool stats_ = false;
    bool flag_subpixel_ = false;
    bool flag_onesided_ = false;
    bool flag_temporal_ = true;
    bool flag_static_snap_ = true;
    bool flag_obmc_ = false;
    bool flag_blend_fallback_ = false;
    bool flag_temporal_smooth_ = false;
    bool flag_depth_occl_ = true;
    bool flag_content_hash_ = true;
    bool flag_global_motion_ = false;
    bool flag_flow_smooth_ = true; // median smooth; skip on finest level always

    uint32_t last_origin_ = 0;
    bool have_origin_ = false;
    unsigned hold_count_ = 0;
    unsigned consecutive_k1_ = 0;
    unsigned pair_k_ = 1; // fields spanned by the current prev→curr pair

    Vulkan::ImageHandle prev_novel_;
    Vulkan::ImageHandle curr_novel_;
    Vulkan::ImageHandle prev_depth_;
    Vulkan::ImageHandle curr_depth_;
    Vulkan::ImageHandle depth_dummy_;

    unsigned scan_w_ = 0;
    unsigned scan_h_ = 0;
    unsigned luma_w_[kMaxLevels] = {};
    unsigned luma_h_[kMaxLevels] = {};
    unsigned num_levels_ = 0;

    Vulkan::ImageHandle luma_a_[kMaxLevels];
    Vulkan::ImageHandle luma_b_[kMaxLevels];
    Vulkan::ImageHandle flow_ab_[kMaxLevels];
    Vulkan::ImageHandle flow_ba_[kMaxLevels];
    Vulkan::ImageHandle flow_tmp_;
    Vulkan::ImageHandle flow_seed_ab_;
    Vulkan::ImageHandle flow_seed_ba_;
    // Temporal store: per-field velocity (luma px / field), not full displacement.
    Vulkan::ImageHandle prev_flow_ab_;
    Vulkan::ImageHandle prev_flow_ba_;
    bool have_prev_flow_ = false;
    Vulkan::ImageHandle output_;
    Vulkan::BufferHandle scene_buf_;

    // Content fingerprint (16x16) for novel-frame detection (async, 1-field lag).
    Vulkan::ImageHandle fp_prev_;
    Vulkan::ImageHandle fp_curr_;
    Vulkan::BufferHandle content_buf_;
    Vulkan::BufferHandle content_readback_;
    Vulkan::Fence content_fence_;
    bool have_fp_ = false;
    bool content_pending_ = false;
    bool content_changed_latched_ = true;

    Vulkan::BufferHandle global_buf_;   // Q8 accumulators (AB+BA)
    Vulkan::BufferHandle global_out_;   // float4 for warp (GPU-only, no readback)
    bool have_global_ = false;

    Vulkan::Program *prog_luma_ = nullptr;
    Vulkan::Program *prog_pyramid_ = nullptr;
    Vulkan::Program *prog_flow_ = nullptr;
    Vulkan::Program *prog_smooth_ = nullptr;
    Vulkan::Program *prog_advect_ = nullptr;
    Vulkan::Program *prog_consistency_ = nullptr;
    Vulkan::Program *prog_temporal_blend_ = nullptr;
    Vulkan::Program *prog_temporal_save_ = nullptr;
    Vulkan::Program *prog_flow_scale_ = nullptr;
    Vulkan::Program *prog_scene_reduce_ = nullptr;
    Vulkan::Program *prog_scene_finalize_ = nullptr;
    Vulkan::Program *prog_warp_ = nullptr;
    Vulkan::Program *prog_fp_ = nullptr;
    Vulkan::Program *prog_content_cmp_ = nullptr;
    Vulkan::Program *prog_content_fin_ = nullptr;
    Vulkan::Program *prog_global_ = nullptr;
    Vulkan::Program *prog_global_fin_ = nullptr;
    bool programs_ready_ = false;

    std::deque<QueueItem> queue_;

    // Latency stats (N64_FRAME_INTERP_STATS=1).
    std::chrono::steady_clock::time_point last_novel_t_{};
    bool have_novel_t_ = false;
    unsigned stats_pairs_ = 0;
    double stats_pair_ms_sum_ = 0.0;
    unsigned stats_k_sum_ = 0;
    std::chrono::steady_clock::time_point stats_window_t_{};

    void ensure_programs(Vulkan::Device &device);
    void ensure_resources(Vulkan::Device &device, unsigned w, unsigned h);
    void build_pyramid(Vulkan::CommandBuffer &cmd, Vulkan::Image &src,
                       Vulkan::ImageHandle *levels);
    void estimate_flow(Vulkan::CommandBuffer &cmd);
    void reduce_scene(Vulkan::CommandBuffer &cmd);
    void apply_consistency(Vulkan::CommandBuffer &cmd);
    void normalize_and_blend_temporal(Vulkan::CommandBuffer &cmd, unsigned k);
    void save_temporal_velocity(Vulkan::CommandBuffer &cmd, unsigned k);
    void append_global_motion(Vulkan::CommandBuffer &cmd);
    void kick_content_fingerprint(Vulkan::Device &device, Vulkan::Image &scanout,
                                  bool do_compare);
    bool poll_content_novel(Vulkan::Device &device);
    void clear_temporal_flow();
    void note_latency_stats(unsigned k);
    Vulkan::ImageHandle warp(Vulkan::Device &device, float phase);
    static void dispatch_2d(Vulkan::CommandBuffer &cmd, unsigned w,
                            unsigned h);
};

} // namespace PRDPWrapper
} // namespace N64

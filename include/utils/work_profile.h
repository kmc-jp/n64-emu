#ifndef UTILS_WORK_PROFILE_H
#define UTILS_WORK_PROFILE_H

#include <chrono>
#include <cstdint>
#include <cstdlib>

namespace N64 {
namespace WorkProfile {

// Wall-time buckets for N64_PROFILE_FRAME. Nested: RspTask contains VuCompute;
// FbCheck may run under CPU and/or RspTask.
enum class Bucket : int {
    RspTask = 0,
    VuCompute,
    FbCheck,
    FbFlush,
    Count,
};

struct Totals {
    double ms[static_cast<int>(Bucket::Count)]{};
    uint64_t events[static_cast<int>(Bucket::Count)]{};
    uint64_t rsp_insns{0};
    uint64_t fb_probes{0}; // check_framebuffers entries with sync_signal != 0
};

inline bool enabled() {
    static const bool on = [] {
        const char *e = std::getenv("N64_PROFILE_FRAME");
        return e && e[0] != '\0' && e[0] != '0';
    }();
    return on;
}

inline Totals &accum() {
    static Totals t;
    return t;
}

inline void add_ms(Bucket b, double ms) {
    if (!enabled())
        return;
    const int i = static_cast<int>(b);
    accum().ms[i] += ms;
    accum().events[i] += 1;
}

inline void add_rsp_insns(uint64_t n) {
    if (!enabled() || n == 0)
        return;
    accum().rsp_insns += n;
}

inline void add_vu_op() {
    if (!enabled())
        return;
    accum().events[static_cast<int>(Bucket::VuCompute)] += 1;
}

inline void add_fb_probe() {
    if (!enabled())
        return;
    accum().fb_probes += 1;
}

inline Totals take_and_reset() {
    Totals out = accum();
    accum() = Totals{};
    return out;
}

class Scoped {
  public:
    explicit Scoped(Bucket b) : bucket_(b), active_(enabled()) {
        if (active_)
            t0_ = std::chrono::steady_clock::now();
    }
    ~Scoped() {
        if (!active_)
            return;
        const double ms =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - t0_)
                .count();
        add_ms(bucket_, ms);
    }

    Scoped(const Scoped &) = delete;
    Scoped &operator=(const Scoped &) = delete;

  private:
    Bucket bucket_;
    bool active_;
    std::chrono::steady_clock::time_point t0_{};
};

inline double ms_of(const Totals &t, Bucket b) {
    return t.ms[static_cast<int>(b)];
}

inline uint64_t events_of(const Totals &t, Bucket b) {
    return t.events[static_cast<int>(b)];
}

} // namespace WorkProfile
} // namespace N64

#endif

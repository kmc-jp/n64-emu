#include "audio/audio.h"
#include "utils/log.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

namespace N64 {
namespace Audio {

namespace {

constexpr int NTSC_DAC_CLOCK = 48681812;
constexpr int HOST_FREQUENCY = 48000;
constexpr int DEFAULT_GUEST_FREQUENCY = 44100;
constexpr int CHANNELS = 2;
constexpr size_t RING_FRAMES = 48000;

constexpr double MAX_LATENCY_FRAMES = 8.0;

bool g_enabled = false;
Sink *g_sink = nullptr;
int g_host_frequency = 0;
int g_guest_frequency = DEFAULT_GUEST_FREQUENCY;
double g_resample_pos = 0.0;
bool g_output_paused = false;
float g_volume = 1.0f;

std::mutex g_mutex;
std::atomic<uint64_t> g_sync_wait_ns{0};
std::vector<int16_t> g_ring;
size_t g_ring_cap_frames = 0;
size_t g_read_frame = 0;
size_t g_write_frame = 0;
size_t g_frames_avail = 0;

size_t max_queued_frames() {
    const size_t per_field =
        static_cast<size_t>(std::ceil(g_host_frequency / 60.0));
    return std::max<size_t>(per_field * static_cast<size_t>(MAX_LATENCY_FRAMES),
                            2048);
}

size_t frames_free_locked() { return g_ring_cap_frames - g_frames_avail; }

void ring_clear_locked() {
    g_read_frame = 0;
    g_write_frame = 0;
    g_frames_avail = 0;
    g_resample_pos = 0.0;
}

void ring_write_locked(const int16_t *frames, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const size_t idx = g_write_frame * CHANNELS;
        g_ring[idx] = frames[i * CHANNELS];
        g_ring[idx + 1] = frames[i * CHANNELS + 1];
        g_write_frame = (g_write_frame + 1) % g_ring_cap_frames;
    }
    g_frames_avail += count;
}

size_t ring_read_locked(int16_t *out, size_t count) {
    const size_t n = std::min(count, g_frames_avail);
    for (size_t i = 0; i < n; ++i) {
        const size_t idx = g_read_frame * CHANNELS;
        out[i * CHANNELS] = g_ring[idx];
        out[i * CHANNELS + 1] = g_ring[idx + 1];
        g_read_frame = (g_read_frame + 1) % g_ring_cap_frames;
    }
    g_frames_avail -= n;
    return n;
}

void set_output_paused(bool pause) {
    if (!g_sink || pause == g_output_paused)
        return;
    g_sink->set_paused(pause);
    g_output_paused = pause;
}

void close_device() {
    if (g_sink)
        g_sink->close();
    g_output_paused = false;
    std::lock_guard lock(g_mutex);
    ring_clear_locked();
}

bool open_device() {
    close_device();

    {
        std::lock_guard lock(g_mutex);
        g_ring_cap_frames = RING_FRAMES;
        g_ring.assign(g_ring_cap_frames * CHANNELS, 0);
        ring_clear_locked();
    }

    if (!g_sink)
        return false;

    const int hz = g_sink->open(HOST_FREQUENCY);
    if (hz <= 0)
        return false;

    g_host_frequency = hz;
    g_output_paused = true;
    g_sink->set_paused(true);
    Utils::info("Audio: Callback device {} Hz (guest {} Hz), non-blocking push",
                g_host_frequency, g_guest_frequency);
    return true;
}

} // namespace

void set_sink(Sink *sink) { g_sink = sink; }

size_t pull_frames(int16_t *out_interleaved, size_t frame_count) {
    std::lock_guard lock(g_mutex);
    const size_t n = ring_read_locked(out_interleaved, frame_count);
    if (n < frame_count) {
        std::memset(out_interleaved + n * CHANNELS, 0,
                    (frame_count - n) * CHANNELS * sizeof(int16_t));
    }
    if (n > 0) {
        if (g_volume <= 0.0f) {
            std::memset(out_interleaved, 0, n * CHANNELS * sizeof(int16_t));
        } else if (g_volume < 1.0f) {
            const float vol = g_volume;
            for (size_t i = 0; i < n * CHANNELS; ++i) {
                const float s = static_cast<float>(out_interleaved[i]) * vol;
                out_interleaved[i] = static_cast<int16_t>(
                    std::clamp(s, -32768.0f, 32767.0f));
            }
        }
    }
    return frame_count;
}

void set_volume(float volume) {
    g_volume = std::clamp(volume, 0.0f, 1.0f);
}

float volume() { return g_volume; }

void notify_space() {}

double take_sync_wait_ms() {
    const uint64_t ns = g_sync_wait_ns.exchange(0, std::memory_order_relaxed);
    return static_cast<double>(ns) / 1e6;
}

void note_sync_wait_ns(uint64_t ns) {
    if (ns)
        g_sync_wait_ns.fetch_add(ns, std::memory_order_relaxed);
}

void init() {
    if (g_enabled)
        return;
    g_enabled = true;
    g_guest_frequency = DEFAULT_GUEST_FREQUENCY;
    open_device();
}

void shutdown() {
    g_enabled = false;
    close_device();
    g_host_frequency = 0;
}

bool enabled() { return g_enabled && g_sink != nullptr && g_host_frequency > 0; }

void set_frequency(int hz) {
    if (!g_enabled)
        return;
    if (hz < 1000)
        hz = DEFAULT_GUEST_FREQUENCY;
    if (hz == g_guest_frequency)
        return;
    g_guest_frequency = hz;
    {
        std::lock_guard lock(g_mutex);
        g_resample_pos = 0.0;
    }
    Utils::debug("Audio: Guest sample rate -> {} Hz", g_guest_frequency);
}

void set_frequency_from_dacrate(uint32_t dacrate) {
    const uint32_t rate = dacrate == 0 ? 1103 : dacrate;
    const int hz = NTSC_DAC_CLOCK / static_cast<int>(rate + 1);
    set_frequency(std::max(hz, 1));
}

void push_samples(std::span<const int16_t> interleaved_stereo) {
    if (!enabled() || interleaved_stereo.size() < 2 || g_host_frequency <= 0 ||
        g_guest_frequency <= 0)
        return;

    const size_t in_frames = interleaved_stereo.size() / 2;
    if (in_frames == 0)
        return;

    if (g_output_paused)
        set_output_paused(false);

    const double ratio = static_cast<double>(g_host_frequency) /
                         static_cast<double>(g_guest_frequency);
    const size_t out_cap =
        static_cast<size_t>(std::ceil(static_cast<double>(in_frames) * ratio)) +
        2;

    thread_local std::vector<int16_t> host;
    host.resize(out_cap * CHANNELS);

    size_t out_frames = 0;
    while (g_resample_pos < static_cast<double>(in_frames)) {
        const size_t i0 =
            std::min(static_cast<size_t>(g_resample_pos), in_frames - 1);
        const size_t i1 = std::min(i0 + 1, in_frames - 1);
        const double frac = g_resample_pos - static_cast<double>(i0);

        const double l0 = interleaved_stereo[i0 * 2 + 0];
        const double r0 = interleaved_stereo[i0 * 2 + 1];
        const double l1 = interleaved_stereo[i1 * 2 + 0];
        const double r1 = interleaved_stereo[i1 * 2 + 1];

        host[out_frames * 2 + 0] =
            static_cast<int16_t>(std::lround(l0 + (l1 - l0) * frac));
        host[out_frames * 2 + 1] =
            static_cast<int16_t>(std::lround(r0 + (r1 - r0) * frac));
        ++out_frames;
        g_resample_pos += 1.0 / ratio;
    }
    g_resample_pos -= static_cast<double>(in_frames);

    {
        std::lock_guard lock(g_mutex);
        const size_t cap = max_queued_frames();
        if (g_frames_avail >= cap)
            return;
        const size_t room = std::min(frames_free_locked(), cap - g_frames_avail);
        const size_t n = std::min(out_frames, room);
        if (n > 0)
            ring_write_locked(host.data(), n);
    }
}

} // namespace Audio
} // namespace N64

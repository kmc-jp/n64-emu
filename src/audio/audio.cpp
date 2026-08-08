#include "audio/audio.h"
#include "utils/log.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace N64 {
namespace Audio {

namespace {

constexpr int NTSC_DAC_CLOCK = 48681812;
constexpr int HOST_FREQUENCY = 48000;
constexpr int DEFAULT_GUEST_FREQUENCY = 44100;
// Soft pacing: wait briefly if the queue is ahead; drop if still too full.
constexpr double SYNC_LATENCY_SEC = 0.080;
constexpr double MAX_LATENCY_SEC = 0.200;
constexpr double MIN_LATENCY_SEC = 0.020;
constexpr int MAX_SYNC_WAIT_MS = 8;

bool g_enabled = false;
SDL_AudioDeviceID g_device = 0;
int g_host_frequency = 0;
int g_guest_frequency = DEFAULT_GUEST_FREQUENCY;
double g_resample_pos = 0.0;
std::vector<int16_t> g_resampled;
std::vector<uint8_t> g_silence;

int bytes_for_seconds(double seconds) {
    return static_cast<int>(g_host_frequency * seconds * 4.0);
}

void close_device() {
    if (g_device != 0) {
        SDL_ClearQueuedAudio(g_device);
        SDL_CloseAudioDevice(g_device);
        g_device = 0;
    }
}

bool open_device() {
    close_device();

    SDL_AudioSpec want{};
    SDL_AudioSpec have{};
    want.freq = HOST_FREQUENCY;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = nullptr;

    // Allow the backend to pick a nearby rate; we resample to whatever we get.
    g_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have,
                                   SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (g_device == 0) {
        Utils::critical("Audio: Failed to open device: {}", SDL_GetError());
        return false;
    }
    if (have.format != AUDIO_S16SYS || have.channels != 2) {
        Utils::critical("Audio: Unexpected device format");
        close_device();
        return false;
    }

    g_host_frequency = have.freq;
    g_resample_pos = 0.0;
    SDL_PauseAudioDevice(g_device, 0);
    Utils::info("Audio: Opened SDL device at {} Hz (guest {} Hz)",
                g_host_frequency, g_guest_frequency);
    return true;
}

} // namespace

void init() {
    if (g_enabled) {
        return;
    }
    g_enabled = true;
    g_guest_frequency = DEFAULT_GUEST_FREQUENCY;
    open_device();
}

void shutdown() {
    close_device();
    g_host_frequency = 0;
    g_enabled = false;
}

bool enabled() { return g_enabled && g_device != 0; }

void set_frequency(int hz) {
    if (!g_enabled) {
        return;
    }
    if (hz < 1000) {
        hz = DEFAULT_GUEST_FREQUENCY;
    }
    if (hz == g_guest_frequency) {
        return;
    }
    g_guest_frequency = hz;
    g_resample_pos = 0.0;
    Utils::info("Audio: Guest sample rate -> {} Hz", g_guest_frequency);
}

void set_frequency_from_dacrate(uint32_t dacrate) {
    const uint32_t rate = dacrate == 0 ? 1103 : dacrate;
    const int hz = NTSC_DAC_CLOCK / static_cast<int>(rate + 1);
    set_frequency(std::max(hz, 1));
}

void push_samples(std::span<const int16_t> interleaved_stereo) {
    if (!enabled() || interleaved_stereo.size() < 2 || g_host_frequency <= 0 ||
        g_guest_frequency <= 0) {
        return;
    }

    const size_t in_frames = interleaved_stereo.size() / 2;
    if (in_frames == 0) {
        return;
    }

    const double ratio =
        static_cast<double>(g_host_frequency) /
        static_cast<double>(g_guest_frequency);

    // Worst-case output frames for this chunk (+2 for fractional phase).
    const size_t out_cap =
        static_cast<size_t>(std::ceil(static_cast<double>(in_frames) * ratio)) +
        2;
    g_resampled.resize(out_cap * 2);

    size_t out_frames = 0;
    while (g_resample_pos < static_cast<double>(in_frames)) {
        const size_t i0 = std::min(static_cast<size_t>(g_resample_pos),
                                   in_frames - 1);
        const size_t i1 = std::min(i0 + 1, in_frames - 1);
        const double frac = g_resample_pos - static_cast<double>(i0);

        const double l0 = interleaved_stereo[i0 * 2 + 0];
        const double r0 = interleaved_stereo[i0 * 2 + 1];
        const double l1 = interleaved_stereo[i1 * 2 + 0];
        const double r1 = interleaved_stereo[i1 * 2 + 1];

        g_resampled[out_frames * 2 + 0] =
            static_cast<int16_t>(std::lround(l0 + (l1 - l0) * frac));
        g_resampled[out_frames * 2 + 1] =
            static_cast<int16_t>(std::lround(r0 + (r1 - r0) * frac));
        ++out_frames;
        g_resample_pos += 1.0 / ratio;
    }
    g_resample_pos -= static_cast<double>(in_frames);

    if (out_frames == 0) {
        return;
    }

    const int nbytes = static_cast<int>(out_frames * 2 * sizeof(int16_t));
    const int max_latency = bytes_for_seconds(MAX_LATENCY_SEC);
    const int sync_latency = bytes_for_seconds(SYNC_LATENCY_SEC);
    const int min_latency = bytes_for_seconds(MIN_LATENCY_SEC);

    // Soft sync: never spin forever (WSL / broken backends may not drain).
    for (int waited = 0;
         waited < MAX_SYNC_WAIT_MS &&
         static_cast<int>(SDL_GetQueuedAudioSize(g_device)) > sync_latency;
         ++waited) {
        SDL_Delay(1);
    }

    int queued = static_cast<int>(SDL_GetQueuedAudioSize(g_device));
    if (queued > max_latency) {
        return;
    }

    // Avoid startup underrun crackle on some hosts.
    if (queued < min_latency) {
        const int pad = (min_latency - queued) & ~3;
        if (pad > 0) {
            g_silence.assign(static_cast<size_t>(pad), 0);
            SDL_QueueAudio(g_device, g_silence.data(), pad);
        }
    }

    if (SDL_QueueAudio(g_device, g_resampled.data(), nbytes) != 0) {
        Utils::debug("Audio: QueueAudio failed: {}", SDL_GetError());
    }
}

} // namespace Audio
} // namespace N64

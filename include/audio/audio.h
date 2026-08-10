#ifndef AUDIO_H
#define AUDIO_H

#include <cstdint>
#include <span>

namespace N64 {
namespace Audio {

// Core ring buffer + resample. Host sink is registered by ui (SDL).
void init();
void shutdown();
bool enabled();

void set_frequency_from_dacrate(uint32_t dacrate);
void set_frequency(int hz);

// Output gain in [0, 1]. Applied when the host pulls frames.
void set_volume(float volume);
float volume();

// Interleaved stereo s16 guest samples (L,R,L,R,...).
void push_samples(std::span<const int16_t> interleaved_stereo);

// Sink interface (implemented by ui/audio_sdl).
struct Sink {
    virtual ~Sink() = default;
    // Open output at preferred host rate; return actual rate or 0 on failure.
    virtual int open(int preferred_hz) = 0;
    virtual void close() = 0;
    virtual void set_paused(bool pause) = 0;
};
void set_sink(Sink *sink);

// Called by the host audio callback to pull interleaved stereo frames.
// Returns frames written (remainder should be zero-filled by caller).
size_t pull_frames(int16_t *out_interleaved, size_t frame_count);
void notify_space();

// Time the emu thread spent blocked pacing against the audio ring. This runs
// inside the CPU execution path, so profilers must subtract it to see real CPU
// cost. Reading drains the accumulator.
double take_sync_wait_ms();

} // namespace Audio
} // namespace N64

#endif

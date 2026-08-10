#ifndef AUDIO_H
#define AUDIO_H

#include <cstdint>
#include <span>

namespace N64 {
namespace Audio {

void init();
void shutdown();
bool enabled();

void set_frequency_from_dacrate(uint32_t dacrate);
void set_frequency(int hz);

void set_volume(float volume);
float volume();

void push_samples(std::span<const int16_t> interleaved_stereo);

struct Sink {
    virtual ~Sink() = default;
    virtual int open(int preferred_hz) = 0;
    virtual void close() = 0;
    virtual void set_paused(bool pause) = 0;
};
void set_sink(Sink *sink);

size_t pull_frames(int16_t *out_interleaved, size_t frame_count);
void notify_space();

double take_sync_wait_ms();
void note_sync_wait_ns(uint64_t ns);

} // namespace Audio
} // namespace N64

#endif

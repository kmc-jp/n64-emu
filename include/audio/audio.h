#ifndef AUDIO_H
#define AUDIO_H

#include <cstdint>
#include <span>

namespace N64 {
namespace Audio {

// Host audio via SDL2 callback thread + ring buffer.
void init();
void shutdown();
bool enabled();

void set_frequency_from_dacrate(uint32_t dacrate);
void set_frequency(int hz);

// Interleaved stereo s16 guest samples (L,R,L,R,...).
void push_samples(std::span<const int16_t> interleaved_stereo);

} // namespace Audio
} // namespace N64

#endif

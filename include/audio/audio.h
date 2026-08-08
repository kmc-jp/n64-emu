#ifndef AUDIO_H
#define AUDIO_H

#include <cstdint>
#include <span>

namespace N64 {
namespace Audio {

// Host audio output via SDL2 (no-op until init()).
void init();
void shutdown();
bool enabled();

// Guest (AI) sample rate from AI_DACRATE, or an explicit Hz value.
void set_frequency_from_dacrate(uint32_t dacrate);
void set_frequency(int hz);

// Interleaved stereo s16 guest samples (L,R,L,R,...).
// Resampled to a fixed host rate; paces to the SDL queue when ahead.
void push_samples(std::span<const int16_t> interleaved_stereo);

} // namespace Audio
} // namespace N64

#endif

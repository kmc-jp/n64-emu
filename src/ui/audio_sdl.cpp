#include "ui/audio_sdl.h"
#include "utils/log.h"
#include <SDL.h>
#include <cstring>

namespace N64 {
namespace Ui {

namespace {

constexpr int CHANNELS = 2;

class SdlAudioSink final : public Audio::Sink {
  public:
    int open(int preferred_hz) override {
        close();
        SDL_AudioSpec want{};
        SDL_AudioSpec have{};
        want.freq = preferred_hz;
        want.format = AUDIO_S16SYS;
        want.channels = CHANNELS;
        want.samples = 1024;
        want.callback = &SdlAudioSink::callback;
        want.userdata = this;

        device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have,
                                     SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        if (device_ == 0) {
            Utils::critical("Audio: Failed to open device: {}", SDL_GetError());
            return 0;
        }
        if (have.format != AUDIO_S16SYS || have.channels != CHANNELS) {
            Utils::critical("Audio: Unexpected device format");
            close();
            return 0;
        }
        return have.freq;
    }

    void close() override {
        if (device_ != 0) {
            SDL_PauseAudioDevice(device_, 1);
            SDL_CloseAudioDevice(device_);
            device_ = 0;
        }
    }

    void set_paused(bool pause) override {
        if (device_ != 0)
            SDL_PauseAudioDevice(device_, pause ? 1 : 0);
    }

  private:
    static void SDLCALL callback(void *userdata, Uint8 *stream, int len) {
        auto *self = static_cast<SdlAudioSink *>(userdata);
        (void)self;
        const size_t want_frames =
            static_cast<size_t>(len) / (sizeof(int16_t) * CHANNELS);
        auto *out = reinterpret_cast<int16_t *>(stream);
        const size_t got = Audio::pull_frames(out, want_frames);
        Audio::notify_space();
        if (got < want_frames) {
            std::memset(out + got * CHANNELS, 0,
                        (want_frames - got) * CHANNELS * sizeof(int16_t));
        }
    }

    SDL_AudioDeviceID device_{0};
};

SdlAudioSink g_sink;

} // namespace

Audio::Sink &sdl_audio_sink() { return g_sink; }

} // namespace Ui
} // namespace N64

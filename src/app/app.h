#pragma once

#include "n64_system/config.h"
#include "n64_system/n64_system.h"
#include "utils/utils.h"
#include <SDL.h>

namespace N64 {
namespace Frontend {

constexpr int WINDOW_WIDTH = 1024;
constexpr int WINDOW_HEIGHT = 768;

class App {
  private:
    N64System::Config config;
    SDL_Window *window{};

  public:
    App(N64System::Config &config) : config(config) {
        if (config.test_mode)
            return;
        SDL_Init(SDL_INIT_EVERYTHING);
        window = SDL_CreateWindow("n64-emu", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                  WINDOW_HEIGHT,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                      SDL_WINDOW_ALLOW_HIGHDPI);
        // TODO: vulkan, wsi, imgui, etc.
    }

    ~App() {
        if (config.test_mode)
            return;
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void run() {
        N64System::set_up(config);

        while (true) {
            N64System::step(config);

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT: {
                    Utils::info("Stopping N64 system");
                    return;
                } break;
                default:
                    break;
                }
            }
            // TODO: update screen every 1/60 seconds
        }
    }
};

} // namespace Frontend
} // namespace N64

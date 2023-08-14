#pragma once

#include "device.hpp"
#include "n64_system/config.h"
#include "n64_system/n64_system.h"
#include "utils/utils.h"
#include "vulkan_headers.hpp"
#include "wsi.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>

namespace N64 {
namespace Frontend {

const char *WINDOW_TITLE = "n64-emu (dev)";
constexpr int WINDOW_WIDTH = 1024;
constexpr int WINDOW_HEIGHT = WINDOW_WIDTH * 3 / 4;

class App {
  private:
    N64System::Config config;
    SDL_Window *window;

  public:
    App(N64System::Config &config) : config(config) {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
            Utils::critical("Failed to initialize SDL: %s", SDL_GetError());
            exit(-1);
        }
        window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                  WINDOW_HEIGHT,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (window == nullptr) {
            Utils::critical("Failed to open Window");
            exit(-1);
        }
        if (!Vulkan::Context::init_loader(
                (PFN_vkGetInstanceProcAddr)
                    SDL_Vulkan_GetVkGetInstanceProcAddr())) {
            Utils::critical("Failed to load Vulkan");
            exit(-1);
        }
        // TODO: vulkan, wsi, etc.
    }

    ~App() {
        SDL_DestroyWindow(window);
        SDL_Vulkan_UnloadLibrary();
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
                    Utils::info("Stopping application");
                    return;
                } break;
                default:
                    break;
                }
            }
        }
    }
};

} // namespace Frontend
} // namespace N64

#pragma once

#include "utils/utils.h"
#include <SDL.h>

namespace N64 {
namespace Frontend {

const char *WINDOW_TITLE = "n64-emu (dev)";
// Aspect ratio is 4:3
constexpr int WINDOW_WIDTH = 1024;
constexpr int WINDOW_HEIGHT = WINDOW_WIDTH * 3 / 4;

class Window {
  private:
    SDL_Window *window{nullptr};
    SDL_Renderer *renderer{nullptr};

  public:
    Window() {
        window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                  WINDOW_HEIGHT,
                                  /*SDL_WINDOW_VULKAN |*/ SDL_WINDOW_RESIZABLE |
                                      SDL_WINDOW_ALLOW_HIGHDPI);
        renderer = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
        if (renderer == nullptr) {
            Utils::critical("Failed to create SDL_Renderer");
            exit(-1);
        }
    }
    ~Window() {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }

    SDL_Window *get_native_sdl_window() const { return window; }

    SDL_Renderer *get_native_sdl_renderer() const { return renderer; }
};

} // namespace Frontend
} // namespace N64
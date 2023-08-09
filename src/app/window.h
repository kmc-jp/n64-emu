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
        // Set render scale for high DPI displays (e.g. Apple Retina)
        const float scale{get_scale()};
        SDL_RenderSetScale(renderer, scale, scale);
    }
    ~Window() {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }

    SDL_Window *get_native_sdl_window() const { return window; }

    SDL_Renderer *get_native_sdl_renderer() const { return renderer; }

    [[nodiscard]] float get_scale() const {
        int window_width{0};
        int window_height{0};
        SDL_GetWindowSize(window, &window_width, &window_height);

        int render_output_width{0};
        int render_output_height{0};
        SDL_GetRendererOutputSize(renderer, &render_output_width,
                                  &render_output_height);

        const auto scale_x{static_cast<float>(render_output_width) /
                           static_cast<float>(window_width)};

        return scale_x;
    }
};

} // namespace Frontend
} // namespace N64
#pragma once

#include "SDL.h"

class Frontend {
  private:
    SDL_Window *window{};

  public:
    Frontend() {
        SDL_Init(SDL_INIT_EVERYTHING);
        window = SDL_CreateWindow("n64-emu", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, 1024, 768,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                      SDL_WINDOW_ALLOW_HIGHDPI);
    }

    ~Frontend() {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void run() {}
};

#pragma once

#include "app/window.h"
#include "n64_system/config.h"
#include "n64_system/n64_system.h"
#include "utils/utils.h"
#include <SDL.h>
// imgui
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>

namespace N64 {
namespace Frontend {

class App {
  private:
    N64System::Config config;
    Window *window = nullptr;

  public:
    App(N64System::Config &config) : config(config) {
        if (config.test_mode)
            return;
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
            Utils::critical("Failed to initialize SDL: %s", SDL_GetError());
            exit(-1);
        }
        window = new Window();
        // TODO: vulkan, wsi, imgui, etc.
    }

    ~App() {
        if (config.test_mode)
            return;
        delete window;
        SDL_Quit();
    }

    void init_imgui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io{ImGui::GetIO()};

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui_ImplSDL2_InitForSDLRenderer(window->get_native_sdl_window(),
                                          window->get_native_sdl_renderer());
        ImGui_ImplSDLRenderer2_Init(window->get_native_sdl_renderer());
    }

    void run() {
        N64System::set_up(config);

        while (true) {
            N64System::step(config);

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                switch (event.type) {
                case SDL_QUIT: {
                    Utils::info("Stopping N64 system");
                    return;
                } break;
                default:
                    break;
                }
            }

            // TODO: remove this
            SDL_SetRenderDrawColor(window->get_native_sdl_renderer(),
                                   // Gray clear color (rgba)
                                   100, 100, 100, 255);
            SDL_RenderClear(window->get_native_sdl_renderer());
            SDL_RenderPresent(window->get_native_sdl_renderer());

            // TODO: update screen every 1/60 seconds
        }
    }
};

} // namespace Frontend
} // namespace N64

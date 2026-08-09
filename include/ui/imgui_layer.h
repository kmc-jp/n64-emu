#pragma once

#include "ui/config_cli.h"
#include "wsi.hpp"
#include <SDL.h>

namespace Vulkan {
class CommandBuffer;
}

namespace N64 {
namespace Ui {

bool imgui_init(SDL_Window *window, Vulkan::WSI &wsi, UiTheme theme);
void imgui_shutdown();
// Rebind the SDL2 platform backend to another window (same ImGui/Vulkan context).
bool imgui_set_sdl_window(SDL_Window *window);
void imgui_apply_theme(UiTheme theme);
UiTheme imgui_current_theme();
void imgui_new_frame();
void imgui_render(Vulkan::CommandBuffer &cmd);
bool imgui_process_event(const SDL_Event &e);
bool imgui_want_capture_keyboard();
bool imgui_want_capture_mouse();

} // namespace Ui
} // namespace N64

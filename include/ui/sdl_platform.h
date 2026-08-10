#pragma once

#include "wsi.hpp"
#include <SDL.h>

namespace N64 {
namespace Ui {

class SDL2Platform : public Vulkan::WSIPlatform {
  public:
    explicit SDL2Platform(SDL_Window *window_);
    VkSurfaceKHR create_surface(VkInstance instance, VkPhysicalDevice) override;
    std::vector<const char *> get_instance_extensions() override;
    uint32_t get_surface_width() override;
    uint32_t get_surface_height() override;
    bool alive(Vulkan::WSI &) override;
    void poll_input() override;

    void set_window(SDL_Window *window_);
    SDL_Window *get_window() const { return window; }

    bool is_alive = true;
    using EventHook = bool (*)(const SDL_Event &e);
    EventHook event_hook = nullptr;

  private:
    SDL_Window *window;
};

unsigned recommended_wsi_thread_indices();
Vulkan::PresentMode recommended_present_mode();
const char *present_mode_name(Vulkan::PresentMode mode);
void ensure_prdp_vulkan_icd();

// Pick the swapchain depth when unset. Deeper absorbs a long field instead of
// dropping it, at the cost of present latency.
void ensure_swapchain_depth();

SDL_Window *create_main_window(const char *title, int width, int height);

} // namespace Ui
} // namespace N64

#pragma once

#include "n64_system/config.h"
#include "wsi.hpp"

struct SDL_Window;

namespace N64 {
namespace Frontend {
class SDL2Platform : public Vulkan::WSIPlatform {
  public:
    SDL2Platform(SDL_Window *window_);
    VkSurfaceKHR create_surface(VkInstance instance, VkPhysicalDevice) override;
    std::vector<const char *> get_instance_extensions() override;
    uint32_t get_surface_width() override;
    uint32_t get_surface_height() override;
    bool alive(Vulkan::WSI &) override;
    void poll_input() override;
    bool is_alive = true;

  private:
    SDL_Window *window;
};

class App {
  private:
    N64System::Config config;
    SDL_Window *window;

  public:
    App(N64System::Config &config);
    ~App();
    void run();
};

} // namespace Frontend
} // namespace N64

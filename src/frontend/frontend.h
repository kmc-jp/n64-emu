#pragma once

#include <SDL.h>
#include <volk.h>

namespace N64 {
namespace Frontend {

constexpr int WINDOW_WIDTH = 1024;
constexpr int WINDOW_HEIGHT = 768;

class Frontend {
  private:
    SDL_Window *window{};
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    uint32_t queueFamily{uint32_t(-1)};
    VkQueue queue{};
    VkPipelineCache pipelineCache{};
    VkDescriptorPool descriptorPool{};
    VkAllocationCallbacks *allocator{};

  public:
    Frontend() {
        SDL_Init(SDL_INIT_EVERYTHING);
        window = SDL_CreateWindow("n64-emu", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                  WINDOW_HEIGHT,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                      SDL_WINDOW_ALLOW_HIGHDPI);
        if (volkInitialize() != VK_SUCCESS) {
            Utils::critical("Failed to initialize Vulkan");
            exit(-1);
        } else {
            Utils::info("Vulkan initialized");
        }
    }

    ~Frontend() {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void update_screen() {}
};

} // namespace Frontend
} // namespace N64

#pragma once

#include "utils.h"
#include "vulkan_headers.hpp"
#include <SDL.h>
#include <volk.h>
#include <wsi.hpp>

namespace N64 {
namespace Frontend {

constexpr int WINDOW_WIDTH = 1024;
constexpr int WINDOW_HEIGHT = 768;

extern Vulkan::WSI *wsi;
extern VkInstance instance;

class SDLWSIPlatform final : public Vulkan::WSIPlatform {
  public:
    SDLWSIPlatform() = default;

    // TODO: add methods
};

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
    uint32_t minImageCount;

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

        /*
        wsi = new Vulkan::WSI();
        wsi->set_backbuffer_srgb(false);
        // wsi->set_platform(new Vulkan::WSIPlatform());
        wsi->set_present_mode(Vulkan::PresentMode::SyncToVBlank);

        Vulkan::Context::SystemHandles handles;
        if (!wsi->init_simple(1, handles)) {
            Utils::critical("Failed to initialize WSI");
            exit(-1);
        }

        instance = wsi->get_context().get_instance();
        physicalDevice = wsi->get_device().get_physical_device();
        device = wsi->get_device().get_device();
        queueFamily = wsi->get_context()
                          .get_queue_info()
                          .family_indices[Vulkan::QUEUE_INDEX_GRAPHICS];
        queue = wsi->get_context()
                    .get_queue_info()
                    .queues[Vulkan::QUEUE_INDEX_GRAPHICS];
        pipelineCache = nullptr;
        descriptorPool = nullptr;
        allocator = nullptr;
        minImageCount = 2;
        */
    }

    ~Frontend() {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void update_screen() {}
};

} // namespace Frontend
} // namespace N64

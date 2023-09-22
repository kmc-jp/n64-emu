#include "app/app.h"
#include "app/parallel_rdp_wrapper.h"
#include "memory/memory.h"
#include "n64_system/n64_system.h"
#include <SDL.h>
#include <SDL_vulkan.h>

namespace N64 {
namespace Frontend {
const char *WINDOW_TITLE = "n64-emu (dev)";
constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = WINDOW_WIDTH * 3 / 4;
// 増やすと軽くなる
constexpr int WSI_NUM_THREADS = 1;

SDL2Platform::SDL2Platform(SDL_Window *window_) : window(window_) {}
VkSurfaceKHR SDL2Platform::create_surface(VkInstance instance,
                                          VkPhysicalDevice) {
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(window, instance, &surface))
        return surface;
    else
        return VK_NULL_HANDLE;
}
std::vector<const char *> SDL2Platform::get_instance_extensions() {
    unsigned instance_ext_count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &instance_ext_count, nullptr);
    std::vector<const char *> instance_names(instance_ext_count);
    SDL_Vulkan_GetInstanceExtensions(window, &instance_ext_count,
                                     instance_names.data());
    return instance_names;
}
uint32_t SDL2Platform::get_surface_width() {
    int w, h;
    SDL_Vulkan_GetDrawableSize(window, &w, &h);
    return w;
}
uint32_t SDL2Platform::get_surface_height() {
    int w, h;
    SDL_Vulkan_GetDrawableSize(window, &w, &h);
    return h;
}
bool SDL2Platform::alive(Vulkan::WSI &) { return is_alive; }
void SDL2Platform::poll_input() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            is_alive = false;
            break;
        default:
            break;
        }
    }
}

App::App(N64System::Config &config) : config(config) {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        Utils::critical("Failed to initialize SDL: %s", SDL_GetError());
        exit(-1);
    }
    window = SDL_CreateWindow(
        WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        Utils::critical("Failed to open Window");
        exit(-1);
    }

    if (volkInitialize() != VK_SUCCESS) {
        Utils::critical("Failed to initialize volk");
        exit(-1);
    }

    if (!Vulkan::Context::init_loader(
            (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr())) {
        Utils::critical("Failed to load Vulkan");
        exit(-1);
    }
    // TODO: vulkan, wsi, etc.
}
App::~App() {
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
}
void App::run() {
    SDL2Platform platform(window);
    Vulkan::WSI wsi;
    wsi.set_platform(&platform);
    // whats this?
    wsi.set_backbuffer_srgb(false);
    wsi.set_present_mode(Vulkan::PresentMode::SyncToVBlank);
    Vulkan::Context::SystemHandles system_handles;
    if (!wsi.init_simple(WSI_NUM_THREADS, system_handles)) {
        Utils::critical("Failed to initialize WSI");
        exit(-1);
    }
    Vulkan::Device &device = wsi.get_device();

    PRDPWrapper::init_prdp(wsi, g_memory().get_rdram().data());

    N64System::set_up(config);

    while (platform.is_alive) {
        N64System::step(config, wsi);

        // Abort when Tab is pressed
        SDL_PumpEvents();
        const uint8_t *state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_TAB])
            Utils::abort("Tab pressed. Aborted");
    }

    PRDPWrapper::fini_prdp();
}
} // namespace Frontend
} // namespace N64

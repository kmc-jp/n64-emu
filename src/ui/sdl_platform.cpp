#include "ui/sdl_platform.h"
#include "utils/log.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace N64 {
namespace Ui {

SDL2Platform::SDL2Platform(SDL_Window *window_) : window(window_) {}

void SDL2Platform::set_window(SDL_Window *window_) { window = window_; }

VkSurfaceKHR SDL2Platform::create_surface(VkInstance instance,
                                          VkPhysicalDevice) {
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(window, instance, &surface))
        return surface;
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
        if (event_hook)
            event_hook(e);
        if (e.type == SDL_QUIT)
            is_alive = false;
    }
}

unsigned recommended_wsi_thread_indices() {
    if (const char *e = getenv("N64_WSI_THREADS"); e && e[0]) {
        char *end = nullptr;
        const unsigned long v = std::strtoul(e, &end, 10);
        if (end != e && v >= 1)
            return static_cast<unsigned>(std::min<unsigned long>(v, 16));
    }
    unsigned hc = std::thread::hardware_concurrency();
    if (hc == 0)
        hc = 4;
    return std::clamp(hc, 2u, 8u);
}

Vulkan::PresentMode recommended_present_mode() {
    const char *e = getenv("N64_PRESENT");
    if (e && e[0]) {
        if (std::strcmp(e, "fifo") == 0 || std::strcmp(e, "vsync") == 0)
            return Vulkan::PresentMode::SyncToVBlank;
        if (std::strcmp(e, "immediate") == 0)
            return Vulkan::PresentMode::UnlockedForceTearing;
        if (std::strcmp(e, "mailbox") != 0)
            Utils::warn("Unknown N64_PRESENT=`{}`; using mailbox", e);
    }
    return Vulkan::PresentMode::UnlockedNoTearing;
}

const char *present_mode_name(Vulkan::PresentMode mode) {
    switch (mode) {
    case Vulkan::PresentMode::SyncToVBlank:
        return "fifo";
    case Vulkan::PresentMode::UnlockedNoTearing:
        return "mailbox";
    case Vulkan::PresentMode::UnlockedForceTearing:
        return "immediate";
    case Vulkan::PresentMode::UnlockedMaybeTear:
        return "mailbox-or-immediate";
    }
    return "unknown";
}

void ensure_prdp_vulkan_icd() {
    constexpr const char *kLvp = "/usr/share/vulkan/icd.d/lvp_icd.json";
    const char *cur = getenv("VK_DRIVER_FILES");
    if (!cur || !*cur)
        cur = getenv("VK_ICD_FILENAMES");

    const bool is_wsl = getenv("WSL_DISTRO_NAME") != nullptr ||
                        getenv("WSL_INTEROP") != nullptr;
    const bool pins_dzn = cur && strstr(cur, "dzn_icd") != nullptr;

    if (!pins_dzn && !(is_wsl && (!cur || !*cur)))
        return;

#ifdef _WIN32
    _putenv_s("VK_DRIVER_FILES", kLvp);
    _putenv_s("VK_ICD_FILENAMES", kLvp);
#else
    setenv("VK_DRIVER_FILES", kLvp, 1);
    setenv("VK_ICD_FILENAMES", kLvp, 1);
#endif
    Utils::warn(
        "Using lavapipe for paraLLEl-RDP (Mesa dzn lacks SSBO 8-bit storage; "
        "native NVIDIA Vulkan is unavailable on WSL)");
}

void ensure_low_latency_swapchain() {
    if (getenv("GRANITE_VULKAN_SWAPCHAIN_IMAGES") &&
        getenv("GRANITE_VULKAN_SWAPCHAIN_IMAGES")[0])
        return;
#ifdef _WIN32
    _putenv_s("GRANITE_VULKAN_SWAPCHAIN_IMAGES", "2");
#else
    setenv("GRANITE_VULKAN_SWAPCHAIN_IMAGES", "2", 0);
#endif
}

SDL_Window *create_main_window(const char *title, int width, int height) {
#ifdef _WIN32
    SetProcessDPIAware();
#endif
    int display_index = 0;
    int mouse_x = 0, mouse_y = 0;
    SDL_GetGlobalMouseState(&mouse_x, &mouse_y);
    const int num_displays = SDL_GetNumVideoDisplays();
    for (int i = 0; i < num_displays; ++i) {
        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(i, &bounds) == 0 && mouse_x >= bounds.x &&
            mouse_x < bounds.x + bounds.w && mouse_y >= bounds.y &&
            mouse_y < bounds.y + bounds.h) {
            display_index = i;
            break;
        }
    }

    return SDL_CreateWindow(
        title, SDL_WINDOWPOS_CENTERED_DISPLAY(display_index),
        SDL_WINDOWPOS_CENTERED_DISPLAY(display_index), width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
}

} // namespace Ui
} // namespace N64

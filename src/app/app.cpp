#include "app/app.h"
#include "app/parallel_rdp_wrapper.h"
#include "audio/audio.h"
#include "memory/memory.h"
#include "n64_system/n64_system.h"
#include "rcp/rsp_thread.h"
#include "utils/log.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace N64 {
namespace Frontend {
const char *WINDOW_TITLE = "n64-emu (dev)";
constexpr int WINDOW_WIDTH = 1600;
constexpr int WINDOW_HEIGHT = WINDOW_WIDTH * 3 / 4;

// Granite allocates per-thread command pools / descriptor caches for this many
// indices. Parallel-RDP mostly uses index 0; keep a small pool sized to the
// host. Override with N64_WSI_THREADS=<n>.
static unsigned recommended_wsi_thread_indices() {
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

// Default mailbox: audio paces the emu; FIFO vsync can double-wait on interlaced
// (2 presents/step) and stall the present path in heavy scenes.
// Override: N64_PRESENT=fifo|mailbox|immediate
static Vulkan::PresentMode recommended_present_mode() {
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

static const char *present_mode_name(Vulkan::PresentMode mode) {
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

// Mesa dzn (Vulkan-on-D3D12) lacks SSBO 8-bit storage required by paraLLEl-RDP.
// On WSL it is often the default discrete GPU and can stall the loader; prefer
// lavapipe there. Native Linux NVIDIA Vulkan is left untouched.
static void ensure_prdp_vulkan_icd() {
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

App::App(N64System::Config &config) : config(config), window(nullptr) {
    if (config.headless)
        return;

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        Utils::critical("Failed to initialize SDL: %s", SDL_GetError());
        exit(-1);
    }
    Audio::init();
    // Prefer the display under the mouse; plain CENTERED uses display 0.
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

    window = SDL_CreateWindow(
        WINDOW_TITLE, SDL_WINDOWPOS_CENTERED_DISPLAY(display_index),
        SDL_WINDOWPOS_CENTERED_DISPLAY(display_index), WINDOW_WIDTH,
        WINDOW_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
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
}
App::~App() {
    N64::Rsp::g_rsp_thread().shutdown();
    Audio::shutdown();
    if (window) {
        SDL_DestroyWindow(window);
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
    }
}

void App::run_headless() {
    Utils::info("Running headless (no window / no Vulkan)");
    N64System::set_up(config);
    // Tests call exit() when finished; otherwise this loops until aborted.
    while (true) {
        N64System::step(config, nullptr);
    }
}

void App::run() {
    if (config.headless) {
        run_headless();
        return;
    }

    ensure_prdp_vulkan_icd();

    SDL2Platform platform(window);
    Vulkan::WSI wsi;
    wsi.set_platform(&platform);
    wsi.set_backbuffer_srgb(false);
    const unsigned wsi_threads = recommended_wsi_thread_indices();
    const Vulkan::PresentMode present_mode = recommended_present_mode();
    wsi.set_present_mode(present_mode);
    Utils::info("WSI: {} thread indices (hw_concurrency={}), present={}",
                wsi_threads, std::thread::hardware_concurrency(),
                present_mode_name(present_mode));
    Vulkan::Context::SystemHandles system_handles;
    if (!wsi.init_simple(wsi_threads, system_handles)) {
        Utils::critical("Failed to initialize WSI");
        exit(-1);
    }

    {
        const VkPhysicalDeviceProperties &gpu =
            wsi.get_device().get_gpu_properties();
        Utils::info("Using Vulkan device: {}", gpu.deviceName);
        if (gpu.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            Utils::warn(
                "Vulkan device is CPU-based ({}); RDP will be very slow",
                gpu.deviceName);
        }
    }

    PRDPWrapper::init_prdp(wsi, g_memory().get_rdram().data(), config.upscale,
                           config.frame_interp);

    N64System::set_up(config);

    while (platform.is_alive) {
        N64System::step(config, &wsi);

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

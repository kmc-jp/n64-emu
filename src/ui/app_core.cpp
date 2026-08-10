#include "ui/app_core.h"
#include "memory/memory.h"
#include "mmio/controller_input.h"
#include "mmio/vi.h"
#include "n64_system/n64_system.h"
#include "rcp/rsp_thread.h"
#include "ui/audio_sdl.h"
#include "ui/input_sdl.h"
#include "ui/sdl_platform.h"
#include "ui/vulkan_devices.h"
#include "utils/log.h"
#include "video/present.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <cstdlib>
#include <filesystem>

namespace N64 {
namespace Ui {

namespace {
constexpr const char *kWindowTitle = "n64-core";
constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = kWindowWidth * 3 / 4;

Vulkan::WSI *g_wsi = nullptr;

// Same root as GUI settings: .../n64-emu/ (not the nested .../n64-emu/n64-emu/).
void init_cart_save_data_dir() {
    if (char *pref = SDL_GetPrefPath("n64-emu", "n64-emu")) {
        std::filesystem::path nested(pref);
        SDL_free(pref);
        const auto dir = nested.parent_path();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::filesystem::remove(nested, ec);
        N64::g_memory().set_data_dir(dir.string());
    }
}

void on_field_present(N64::Mmio::VI::VI &vi) {
    if (!g_wsi)
        return;
    poll_and_inject_controller(false);
    Video::present_field(*g_wsi, vi, false);
}

void host_controller_poll() { poll_and_inject_controller(false); }

N64System::PresentCounters on_present_stats() {
    const auto s = Video::take_present_stats();
    return {s.presented, s.skipped};
}
} // namespace

AppCore::AppCore(N64System::Config &config_)
    : config(config_), window(nullptr) {
    // SDL_GetPrefPath needs SDL; Init is cheap enough for headless too.
    if (SDL_Init(SDL_INIT_EVENTS) != 0) {
        Utils::warn("SDL_Init failed (save path may fall back): {}",
                    SDL_GetError());
    } else {
        init_cart_save_data_dir();
    }

    if (config.headless)
        return;

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        Utils::critical("Failed to initialize SDL: %s", SDL_GetError());
        exit(-1);
    }
    Audio::set_sink(&sdl_audio_sink());
    Audio::init();
    input_init();
    Input::set_host_poll(&host_controller_poll);

    window = create_main_window(kWindowTitle, kWindowWidth, kWindowHeight);
    if (!window) {
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

AppCore::~AppCore() {
    Input::set_host_poll(nullptr);
    N64::Rsp::g_rsp_thread().shutdown();
    input_shutdown();
    Audio::shutdown();
    if (window) {
        SDL_DestroyWindow(window);
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
    }
}

void AppCore::run_headless() {
    Utils::info("Running headless (no window / no Vulkan)");
    N64System::set_field_present(nullptr);
    N64System::set_present_stats_fn(nullptr);
    N64System::set_up(config);
    while (true)
        N64System::step(config);
}

void AppCore::run_windowed() {
    ensure_prdp_vulkan_icd();
    ensure_low_latency_swapchain();

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
    if (!init_wsi_with_device(wsi, wsi_threads, system_handles,
                             config.vulkan_device)) {
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

    Video::init_video(wsi, g_memory().get_rdram().data(), config.upscale,
               config.frame_interp);
    g_wsi = &wsi;
    N64System::set_field_present(&on_field_present);
    N64System::set_present_stats_fn(&on_present_stats);
    N64System::set_up(config);

    while (platform.is_alive) {
        N64System::step(config);

        SDL_PumpEvents();
        const uint8_t *state = SDL_GetKeyboardState(nullptr);
        if (state[SDL_SCANCODE_TAB])
            Utils::abort("Tab pressed. Aborted");
    }

    N64System::shutdown();
    N64System::set_field_present(nullptr);
    N64System::set_present_stats_fn(nullptr);
    Video::fini_video();
    g_wsi = nullptr;
}

void AppCore::run() {
    if (config.headless)
        run_headless();
    else
        run_windowed();
}

} // namespace Ui
} // namespace N64

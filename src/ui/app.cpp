#include "ui/app.h"
#include "app_identity.h"
#include "memory/memory.h"
#include "mmio/controller_input.h"
#include "mmio/vi.h"
#include "n64_system/n64_system.h"
#include "rcp/rsp_thread.h"
#include "ui/audio_sdl.h"
#include "ui/gui.h"
#include "ui/imgui_layer.h"
#include "ui/input_sdl.h"
#include "ui/recent_roms.h"
#include "ui/sdl_platform.h"
#include "ui/vulkan_devices.h"
#include "ui/file_dialog.h"
#include "utils/log.h"
#include "video/present.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace N64 {
namespace Ui {

namespace {
constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = kWindowWidth * 3 / 4;

GuiState g_gui{};
Vulkan::WSI *g_wsi = nullptr;
SDL_Window *g_menu_window = nullptr;
SDL_Window *g_game_window = nullptr;

void prepare_imgui() {
    imgui_new_frame();
    gui_draw(g_gui);
}

void note_fps(uint32_t origin, bool presented) {
    using clock = std::chrono::steady_clock;
    static clock::time_point window_start = clock::now();
    static uint64_t origin_changes = 0;
    static uint64_t presents = 0;
    static uint32_t last_origin = 0;
    static bool have_origin = false;

    if (!have_origin) {
        last_origin = origin;
        have_origin = true;
    } else if (origin != last_origin) {
        ++origin_changes;
        last_origin = origin;
    }
    if (presented)
        ++presents;

    const auto now = clock::now();
    const double elapsed =
        std::chrono::duration<double>(now - window_start).count();
    if (elapsed >= 1.0) {
        g_gui.fps_game = static_cast<float>(origin_changes / elapsed);
        g_gui.fps_display = static_cast<float>(presents / elapsed);
        origin_changes = 0;
        presents = 0;
        window_start = now;
    }
}

void on_field_present(N64::Mmio::VI::VI &vi) {
    if (!g_wsi)
        return;
    g_wsi->get_platform().poll_input();
    // Also refresh here so ImGui / title path stays responsive between PIF polls.
    poll_and_inject_controller(imgui_want_capture_keyboard());
    prepare_imgui();
    // While playing, skip duplicate-VI presents to keep the swapchain shallow.
    // Force present when a settings/about window needs a live UI refresh.
    const bool force_ui =
        g_gui.mode != AppMode::Running || g_gui.show_video_settings ||
        g_gui.show_emu_settings || g_gui.show_audio_settings ||
        g_gui.show_controller_settings || g_gui.show_about ||
        g_gui.menu_bar_active;
    const uint32_t origin = vi.reg_origin;
    if (!Video::present_field(*g_wsi, vi, force_ui)) {
        imgui_abandon_frame();
        note_fps(origin, false);
    } else {
        note_fps(origin, true);
    }
}

void host_controller_poll() {
    poll_and_inject_controller(imgui_want_capture_keyboard());
}

N64System::PresentCounters on_present_stats() {
    const auto s = Video::take_present_stats();
    return {s.presented, s.skipped};
}

void on_overlay_draw(Vulkan::CommandBuffer &cmd) { imgui_render(cmd); }

void update_window_title(SDL_Window *target) {
    if (!target)
        return;

    std::string title = g_memory().rom.get_image_name();
    if (title.empty() && g_gui.config && !g_gui.config->rom_filepath.empty()) {
        title = std::filesystem::path(g_gui.config->rom_filepath)
                    .filename()
                    .string();
    }
    if (title.empty())
        title = kWindowTitle;
    SDL_SetWindowTitle(target, title.c_str());
}

bool switch_present_window(Vulkan::WSI &wsi, SDL2Platform &platform,
                           SDL_Window *target) {
    if (!target)
        return false;

    wsi.deinit_surface_and_swapchain();
    platform.set_window(target);
    if (!imgui_set_sdl_window(target))
        return false;

    auto &ctx = wsi.get_context();
    VkSurfaceKHR surface =
        platform.create_surface(ctx.get_instance(), ctx.get_gpu());
    if (surface == VK_NULL_HANDLE) {
        Utils::critical("Failed to create Vulkan surface for new window");
        return false;
    }
    wsi.reinit_surface_and_swapchain(surface);
    return true;
}

bool event_hook(const SDL_Event &e) {
    imgui_process_event(e);
    input_handle_event(e);

    if (e.type == SDL_WINDOWEVENT &&
        e.window.event == SDL_WINDOWEVENT_CLOSE) {
        const Uint32 id = e.window.windowID;
        if (g_game_window && id == SDL_GetWindowID(g_game_window)) {
            g_gui.request_stop = true;
            return true;
        }
        if (g_menu_window && id == SDL_GetWindowID(g_menu_window)) {
            g_gui.request_quit = true;
            return true;
        }
    }

    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_o &&
        (e.key.keysym.mod & KMOD_CTRL)) {
        open_rom_dialog(g_gui);
        return true;
    }

    if (e.type == SDL_DROPFILE) {
        const char *dropped = e.drop.file;
        if (dropped) {
            const std::string path(dropped);
            SDL_free(e.drop.file);
            if (is_n64_rom_path(path) && std::filesystem::exists(path)) {
                open_rom_file(g_gui, path);
            } else {
                Utils::warn("Ignored drop (not an N64 ROM): {}", path);
            }
        }
        return true;
    }

    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F11 &&
        !e.key.repeat) {
        SDL_Window *target = g_game_window ? g_game_window : g_menu_window;
        if (target) {
            const bool fs =
                (SDL_GetWindowFlags(target) &
                 (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
            SDL_SetWindowFullscreen(target,
                                    fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        return true;
    }
    return false;
}

bool try_start_game(Vulkan::WSI &wsi, SDL2Platform &platform, uint8_t *rdram) {
    if (!g_gui.request_start || !g_gui.config ||
        g_gui.config->rom_filepath.empty())
        return false;
    g_gui.request_start = false;

    if (!g_game_window) {
        g_game_window =
            create_main_window(kGameWindowTitle, kWindowWidth, kWindowHeight);
        if (!g_game_window) {
            Utils::critical("Failed to open game window");
            return false;
        }
        if (!switch_present_window(wsi, platform, g_game_window)) {
            SDL_DestroyWindow(g_game_window);
            g_game_window = nullptr;
            return false;
        }
        if (g_menu_window)
            SDL_HideWindow(g_menu_window);
    }

    Video::init_video(wsi, rdram, g_gui.config->upscale, g_gui.config->frame_interp);
    Video::set_frame_interp_mode(
        g_gui.config->frame_interp_mode ==
                N64System::FrameInterpMode::Extrapolate
            ? Video::FrameInterpMode::Extrapolate
            : Video::FrameInterpMode::Bidirectional);
    N64System::set_up(*g_gui.config);
    remember_recent_rom(g_gui.recent_roms, g_gui.config->rom_filepath);
    g_gui.mode = AppMode::Running;
    update_window_title(g_game_window);
    return true;
}

void stop_game(Vulkan::WSI &wsi, SDL2Platform &platform) {
    N64System::shutdown();
    Video::fini_video();
    g_gui.mode = AppMode::Menu;
    g_gui.request_stop = false;

    if (g_game_window) {
        if (g_menu_window) {
            if (!switch_present_window(wsi, platform, g_menu_window))
                Utils::critical("Failed to restore menu window surface");
            SDL_ShowWindow(g_menu_window);
            SDL_RaiseWindow(g_menu_window);
        }
        SDL_DestroyWindow(g_game_window);
        g_game_window = nullptr;
    }

    if (g_menu_window)
        SDL_SetWindowTitle(g_menu_window, kWindowTitle);
}

} // namespace

App::App(N64System::Config &config_, UiSettings &ui_settings_)
    : config(config_), ui_settings(ui_settings_), window(nullptr),
      game_window(nullptr) {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        Utils::critical("Failed to initialize SDL: %s", SDL_GetError());
        exit(-1);
    }
    Audio::set_sink(&sdl_audio_sink());
    Audio::init();
    Audio::set_volume(ui_settings.audio_volume);
    set_key_binds(ui_settings.key_binds);
    set_pad_binds(ui_settings.pad_binds);
    input_init();
    Input::set_host_poll(&host_controller_poll);

    window = create_main_window(kWindowTitle, kWindowWidth, kWindowHeight);
    if (!window) {
        Utils::critical("Failed to open Window");
        exit(-1);
    }
    g_menu_window = window;

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
    Input::set_host_poll(nullptr);
    imgui_shutdown();
    N64::Rsp::g_rsp_thread().shutdown();
    input_shutdown();
    Audio::shutdown();
    if (game_window) {
        SDL_DestroyWindow(game_window);
        game_window = nullptr;
        g_game_window = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        g_menu_window = nullptr;
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
    }
}

void App::run() {
    ensure_prdp_vulkan_icd();
    ensure_swapchain_depth();

    SDL2Platform platform(window);
    platform.event_hook = &event_hook;
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

    if (!imgui_init(window, wsi, ui_settings.theme))
        exit(-1);
    Video::set_overlay_draw(&on_overlay_draw);

    uint8_t *rdram = g_memory().get_rdram().data();
    g_wsi = &wsi;
    g_gui.config = &config;
    g_gui.ui_settings = &ui_settings;
    g_gui.wsi = &wsi;
    g_gui.rdram = rdram;
    g_gui.applied_vulkan_device = config.vulkan_device;
    g_gui.recent_roms = load_recent_roms();
    g_gui.mode = AppMode::Menu;

    N64System::set_field_present(&on_field_present);
    N64System::set_present_stats_fn(&on_present_stats);

    if (!config.rom_filepath.empty())
        g_gui.request_start = true;

    while (platform.is_alive && !g_gui.request_quit) {
        if (g_gui.mode == AppMode::Menu) {
            platform.poll_input();
            if (!platform.is_alive || g_gui.request_quit)
                break;

            poll_and_inject_controller(imgui_want_capture_keyboard());
            prepare_imgui();
            Video::present_ui_only(wsi);

            try_start_game(wsi, platform, rdram);
            // Sync member used by destructor if start opened a game window.
            game_window = g_game_window;
            SDL_Delay(16);
            continue;
        }

        N64System::step(config);

        if (g_gui.request_stop || g_gui.request_quit) {
            const bool quit = g_gui.request_quit;
            stop_game(wsi, platform);
            game_window = g_game_window;
            if (quit)
                break;
            try_start_game(wsi, platform, rdram);
            game_window = g_game_window;
        }
    }

    if (g_gui.mode == AppMode::Running)
        stop_game(wsi, platform);
    game_window = g_game_window;

    N64System::set_field_present(nullptr);
    N64System::set_present_stats_fn(nullptr);
    Video::set_overlay_draw(nullptr);
    imgui_shutdown();
    g_wsi = nullptr;
}

} // namespace Ui
} // namespace N64

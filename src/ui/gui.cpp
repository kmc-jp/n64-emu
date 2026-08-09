#include "ui/gui.h"
#include "imgui.h"
#include "rdp/rdp_core.h"
#include "ui/config_toml.h"
#include "ui/imgui_layer.h"
#include "ui/sdl_platform.h"
#include "ui/win32_file_dialog.h"
#include "video/present.h"
#include <SDL.h>
#include <filesystem>
#include <string>

namespace N64 {
namespace Ui {

namespace {

namespace fs = std::filesystem;

void save_settings(GuiState &state) {
    if (state.config && state.ui_settings)
        save_toml(*state.config, *state.ui_settings);
}

SDL_Window *current_window(GuiState &state) {
    if (!state.wsi)
        return nullptr;
    return static_cast<SDL2Platform &>(state.wsi->get_platform()).get_window();
}

bool window_is_fullscreen(SDL_Window *window) {
    if (!window)
        return false;
    const Uint32 flags = SDL_GetWindowFlags(window);
    return (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) !=
           0;
}

void set_fullscreen(GuiState &state, bool fullscreen) {
    SDL_Window *window = current_window(state);
    if (!window || window_is_fullscreen(window) == fullscreen)
        return;
    SDL_SetWindowFullscreen(window,
                            fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void set_theme(GuiState &state, UiTheme theme) {
    if (!state.ui_settings || state.ui_settings->theme == theme)
        return;
    state.ui_settings->theme = theme;
    imgui_apply_theme(theme);
    save_settings(state);
}

void draw_video_settings(GuiState &state) {
    if (!state.show_video_settings)
        return;

    ImGui::SetNextWindowSize(ImVec2(400, 160), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Video Settings", &state.show_video_settings)) {
        ImGui::End();
        return;
    }

    auto &cfg = *state.config;

    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("Upscale",
                          (std::to_string(cfg.upscale) + "x").c_str())) {
        for (unsigned u : {1u, 2u, 4u, 8u}) {
            const bool sel = cfg.upscale == u;
            if (ImGui::Selectable((std::to_string(u) + "x").c_str(), sel) &&
                !sel) {
                cfg.upscale = u;
                if (Rdp::ready() && state.wsi && state.rdram)
                    Video::reinit_rdp(*state.wsi, state.rdram, cfg.upscale,
                                      cfg.frame_interp);
                save_settings(state);
            }
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    bool fi = cfg.frame_interp;
    if (ImGui::Checkbox("Frame interpolation", &fi)) {
        cfg.frame_interp = fi;
        if (Rdp::ready())
            Video::set_frame_interp_enabled(fi);
        save_settings(state);
    }

    ImGui::End();
}

void draw_emu_settings(GuiState &state) {
    if (!state.show_emu_settings)
        return;

    ImGui::SetNextWindowSize(ImVec2(400, 160), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Emulation Settings", &state.show_emu_settings)) {
        ImGui::End();
        return;
    }

    auto &cfg = *state.config;
    const bool running = state.mode == AppMode::Running;

    bool jit = cfg.cpu_backend == N64System::CpuBackend::Jit;
    if (ImGui::Checkbox("CPU JIT", &jit)) {
        cfg.cpu_backend =
            jit ? N64System::CpuBackend::Jit : N64System::CpuBackend::Interpreter;
        save_settings(state);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip(
            "Dynarec for the main CPU. Faster than the interpreter; "
            "turn off for debugging.");

    bool rsp = cfg.rsp_thread;
    if (ImGui::Checkbox("Multi-threaded RSP", &rsp)) {
        cfg.rsp_thread = rsp;
        save_settings(state);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip(
            "Run the RSP (audio/graphics helper CPU) on a background thread.\n"
            "Usually faster; turn off if you hit timing issues.");

    if (running)
        ImGui::TextDisabled(
            "CPU JIT / Multi-threaded RSP apply after Stop / next Open ROM.");

    ImGui::End();
}

} // namespace

void gui_draw(GuiState &state) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open ROM...", "Ctrl+O"))
                win32_open_rom_dialog(state);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                state.request_quit = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Emulation")) {
            const bool running = state.mode == AppMode::Running;
            if (ImGui::MenuItem("Stop", nullptr, false, running))
                state.request_stop = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Video Settings"))
                state.show_video_settings = true;
            if (ImGui::MenuItem("Emulation Settings"))
                state.show_emu_settings = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            SDL_Window *window = current_window(state);
            const bool fs = window_is_fullscreen(window);
            if (ImGui::MenuItem("Fullscreen", "F11", fs))
                set_fullscreen(state, true);
            if (ImGui::MenuItem("Window", nullptr, !fs))
                set_fullscreen(state, false);
            ImGui::Separator();
            if (ImGui::BeginMenu("Theme")) {
                const UiTheme theme = state.ui_settings
                                          ? state.ui_settings->theme
                                          : imgui_current_theme();
                if (ImGui::MenuItem("Light", nullptr,
                                    theme == UiTheme::Light))
                    set_theme(state, UiTheme::Light);
                if (ImGui::MenuItem("Dark", nullptr, theme == UiTheme::Dark))
                    set_theme(state, UiTheme::Dark);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (state.config && !state.config->rom_filepath.empty()) {
            const fs::path rom(state.config->rom_filepath);
            const std::string label = "ROM: " + rom.filename().string();
            const float w = ImGui::CalcTextSize(label.c_str()).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - w - 16.f);
            ImGui::TextUnformatted(label.c_str());
        }

        ImGui::EndMainMenuBar();
    }

    draw_video_settings(state);
    draw_emu_settings(state);
}

} // namespace Ui
} // namespace N64

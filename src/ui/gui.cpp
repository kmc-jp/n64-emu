#include "ui/gui.h"
#include "about_info.h"
#include "imgui.h"
#include "rdp/rdp_core.h"
#include "ui/config_toml.h"
#include "ui/imgui_layer.h"
#include "ui/sdl_platform.h"
#include "ui/vulkan_devices.h"
#include "ui/file_dialog.h"
#include "video/present.h"
#include <SDL.h>
#include <filesystem>
#include <string>

namespace N64 {
namespace Ui {

namespace {

void save_settings(GuiState &state) {
    if (state.config && state.ui_settings)
        save_toml(*state.config, *state.ui_settings);
}

void open_rom_path(GuiState &state, const std::string &path) {
    if (!state.config || path.empty())
        return;
    state.config->rom_filepath = path;
    if (state.ui_settings) {
        const auto dir = std::filesystem::path(path).parent_path();
        if (!dir.empty()) {
            state.ui_settings->last_rom_dir = dir.string();
            save_settings(state);
        }
    }
    state.request_start = true;
    if (state.mode == AppMode::Running)
        state.request_stop = true;
}

void draw_recents_menu(GuiState &state) {
    if (!ImGui::BeginMenu("Recents", !state.recent_roms.empty()))
        return;

    for (const std::string &path : state.recent_roms) {
        const std::string label =
            std::filesystem::path(path).filename().string();
        const bool exists = std::filesystem::exists(path);
        if (!exists)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem(label.c_str(), nullptr, false, exists))
            open_rom_path(state, path);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", path.c_str());
        if (!exists)
            ImGui::EndDisabled();
    }

    ImGui::EndMenu();
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

    ImGui::SetNextWindowSize(ImVec2(520, 240), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Video Settings", &state.show_video_settings,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    auto &cfg = *state.config;

    std::vector<VulkanDeviceInfo> devices;
    if (state.wsi) {
        devices = enumerate_vulkan_devices(
            state.wsi->get_context().get_instance());
    }

    std::string combo_label = "Auto";
    if (!cfg.vulkan_device.empty()) {
        if (const VulkanDeviceInfo *sel =
                find_device_by_uuid(devices, cfg.vulkan_device)) {
            combo_label =
                sel->name + " (" + vulkan_device_type_name(sel->type) + ")";
        } else {
            combo_label = cfg.vulkan_device;
        }
    }

    const VulkanDeviceInfo *preferred =
        find_device_by_uuid(devices, cfg.vulkan_device);

    // Label column sizes to content; control column takes the rest.
    if (ImGui::BeginTable("##video_settings", 2,
                          ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Graphics accelerator:");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##graphics_accelerator", combo_label.c_str())) {
            const bool auto_sel = cfg.vulkan_device.empty();
            if (ImGui::Selectable("Auto", auto_sel) && !auto_sel) {
                cfg.vulkan_device.clear();
                save_settings(state);
            }
            if (auto_sel)
                ImGui::SetItemDefaultFocus();

            for (const auto &dev : devices) {
                const std::string label =
                    dev.name + " (" + vulkan_device_type_name(dev.type) + ")" +
                    (dev.supports_prdp ? "" : " [unsupported]");
                const bool selected =
                    preferred && preferred->handle == dev.handle;
                if (!dev.supports_prdp) {
                    ImGui::BeginDisabled();
                    ImGui::Selectable(label.c_str(), selected);
                    ImGui::EndDisabled();
                    continue;
                }
                if (ImGui::Selectable(label.c_str(), selected) && !selected) {
                    cfg.vulkan_device = dev.uuid;
                    save_settings(state);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (cfg.vulkan_device != state.applied_vulkan_device) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("Applies after restart.");
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Upscale:");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::BeginCombo("##upscale",
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

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Frame interpolation:");
        ImGui::TableSetColumnIndex(1);
        bool fi = cfg.frame_interp;
        if (ImGui::Checkbox("##frame_interp", &fi)) {
            cfg.frame_interp = fi;
            if (Rdp::ready())
                Video::set_frame_interp_enabled(fi);
            save_settings(state);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

void draw_emu_settings(GuiState &state) {
    if (!state.show_emu_settings)
        return;

    ImGui::SetNextWindowSize(ImVec2(400, 180), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Emulation Settings", &state.show_emu_settings,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    auto &cfg = *state.config;
    const bool running = state.mode == AppMode::Running;

    if (ImGui::BeginTable("##emu_settings", 2,
                          ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("CPU JIT:");
        ImGui::TableSetColumnIndex(1);
        bool jit = cfg.cpu_backend == N64System::CpuBackend::Jit;
        if (ImGui::Checkbox("##cpu_jit", &jit)) {
            cfg.cpu_backend = jit ? N64System::CpuBackend::Jit
                                  : N64System::CpuBackend::Interpreter;
            save_settings(state);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip(
                "Dynarec for the main CPU. Faster than the interpreter; "
                "turn off for debugging.");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Multi-threaded RSP:");
        ImGui::TableSetColumnIndex(1);
        bool rsp = cfg.rsp_thread;
        if (ImGui::Checkbox("##rsp_thread", &rsp)) {
            cfg.rsp_thread = rsp;
            save_settings(state);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip(
                "Run the RSP (audio/graphics helper CPU) on a background "
                "thread.\n"
                "Usually faster; turn off if you hit timing issues.");

        ImGui::EndTable();
    }

    if (running)
        ImGui::TextDisabled(
            "CPU JIT / Multi-threaded RSP apply after Stop / next Open ROM.");

    ImGui::End();
}

void draw_about(GuiState &state) {
    if (!state.show_about)
        return;

    ImGui::SetNextWindowSize(ImVec2(420, 200), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("About", &state.show_about,
                      ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    const ImGuiStyle &style = ImGui::GetStyle();

    ImGui::PushFont(nullptr, style.FontSizeBase * 1.45f);
    ImGui::TextUnformatted("n64-emu");
    ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    ImGui::TextUnformatted("Nintendo 64 Emulator");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Revision: %s", kGitHash);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", kGitHashFull);

    ImGui::TextUnformatted("Website:");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL("github.com/kmc-jp/n64-emu",
                           "https://github.com/kmc-jp/n64-emu");

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::PushFont(nullptr, style.FontSizeBase * 0.85f);
    ImGui::TextDisabled("Copyright (c) 2023-2026 n64-emu contributors");
    ImGui::PopFont();

    ImGui::End();
}

} // namespace

void gui_draw(GuiState &state) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open ROM...", "Ctrl+O"))
                open_rom_dialog(state);
            draw_recents_menu(state);
            if (ImGui::MenuItem("Open n64-emu folder"))
                open_settings_dir();
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

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About n64-emu"))
                state.show_about = true;
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    draw_video_settings(state);
    draw_emu_settings(state);
    draw_about(state);
}

} // namespace Ui
} // namespace N64

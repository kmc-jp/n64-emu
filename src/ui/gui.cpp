#include "ui/gui.h"
#include "app_identity.h"
#include "audio/audio.h"
#include "imgui.h"
#include "rdp/rdp_core.h"
#include "ui/config_toml.h"
#include "ui/file_dialog.h"
#include "ui/imgui_layer.h"
#include "ui/input_sdl.h"
#include "ui/sdl_platform.h"
#include "ui/vulkan_devices.h"
#include "video/present.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <utility>

namespace N64 {
namespace Ui {

namespace {

void save_settings(GuiState &state) {
    if (state.config && state.ui_settings)
        save_toml(*state.config, *state.ui_settings);
}

void draw_recents_menu(GuiState &state) {
    if (!ImGui::BeginMenu("Recents", !state.recent_roms.empty()))
        return;

    for (size_t i = 0; i < state.recent_roms.size(); ++i) {
        const std::string &path = state.recent_roms[i];
        const std::string label =
            std::filesystem::path(path).filename().string();
        const bool exists = std::filesystem::exists(path);
        ImGui::PushID(static_cast<int>(i));
        if (!exists)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem(label.c_str(), nullptr, false, exists))
            open_rom_file(state, path);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", path.c_str());
        if (!exists)
            ImGui::EndDisabled();
        ImGui::PopID();
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

// While a game is running the menu bar stays hidden until the mouse touches the
// top edge of the window, and remains up while one of its menus is open.
bool menu_bar_visible(bool auto_hide, bool menu_open) {
    if (!auto_hide)
        return true;
    if (menu_open)
        return true;

    const ImGuiIO &io = ImGui::GetIO();
    if (!ImGui::IsMousePosValid(&io.MousePos))
        return false;

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    const float reveal_height = ImGui::GetFrameHeight();
    return io.MousePos.y >= vp->Pos.y &&
           io.MousePos.y <= vp->Pos.y + reveal_height &&
           io.MousePos.x >= vp->Pos.x && io.MousePos.x < vp->Pos.x + vp->Size.x;
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

    ImGui::SetNextWindowSize(ImVec2(520, 280), ImGuiCond_FirstUseEver);
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
        const char *fi_label = "None";
        if (cfg.frame_interp) {
            switch (cfg.frame_interp_mode) {
            case N64System::FrameInterpMode::LinearBlend:
                fi_label = "Linear blend (interpolation)";
                break;
            case N64System::FrameInterpMode::Extrapolate:
                fi_label = "GFFE (extrapolation)";
                break;
            case N64System::FrameInterpMode::OpticalFlow:
            default:
                fi_label = "Optical flow (interpolation)";
                break;
            }
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##frame_interp", fi_label)) {
            const bool none_sel = !cfg.frame_interp;
            if (ImGui::Selectable("None", none_sel) && !none_sel) {
                cfg.frame_interp = false;
                if (Rdp::ready())
                    Video::set_frame_interp_enabled(false);
                save_settings(state);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("No extra frames.");
            if (none_sel)
                ImGui::SetItemDefaultFocus();

            const bool linear_sel =
                cfg.frame_interp &&
                cfg.frame_interp_mode == N64System::FrameInterpMode::LinearBlend;
            if (ImGui::Selectable("Linear blend (interpolation)", linear_sel) &&
                !linear_sel) {
                cfg.frame_interp = true;
                cfg.frame_interp_mode = N64System::FrameInterpMode::LinearBlend;
                if (Rdp::ready()) {
                    Video::set_frame_interp_mode(
                        Video::FrameInterpMode::LinearBlend);
                    Video::set_frame_interp_enabled(true);
                }
                save_settings(state);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip(
                    "Waits for the next frame, then RGB-crossfades "
                    "previous→next. Uses both frames, so display is delayed "
                    "by ~1 source frame (may ghost on motion).");
            if (linear_sel)
                ImGui::SetItemDefaultFocus();

            const bool flow_sel =
                cfg.frame_interp &&
                cfg.frame_interp_mode == N64System::FrameInterpMode::OpticalFlow;
            if (ImGui::Selectable("Optical flow (interpolation)", flow_sel) &&
                !flow_sel) {
                cfg.frame_interp = true;
                cfg.frame_interp_mode = N64System::FrameInterpMode::OpticalFlow;
                if (Rdp::ready()) {
                    Video::set_frame_interp_mode(
                        Video::FrameInterpMode::OpticalFlow);
                    Video::set_frame_interp_enabled(true);
                }
                save_settings(state);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip(
                    "Waits for the next frame, then motion-compensated warp. "
                    "Uses both frames, so display is delayed by ~1 source "
                    "frame.");
            if (flow_sel)
                ImGui::SetItemDefaultFocus();

            const bool one_sel =
                cfg.frame_interp &&
                cfg.frame_interp_mode == N64System::FrameInterpMode::Extrapolate;
            if (ImGui::Selectable("GFFE (extrapolation)", one_sel) && !one_sel) {
                cfg.frame_interp = true;
                cfg.frame_interp_mode = N64System::FrameInterpMode::Extrapolate;
                if (Rdp::ready()) {
                    Video::set_frame_interp_mode(
                        Video::FrameInterpMode::Extrapolate);
                    Video::set_frame_interp_enabled(true);
                }
                save_settings(state);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip(
                    "G-buffer Free Frame Extrapolation.\n"
                    "Shows each new frame immediately; fills duplicates by "
                    "forward-predicting from history only (no extra frame "
                    "delay; may ghost on abrupt shading/motion).");
            if (one_sel)
                ImGui::SetItemDefaultFocus();
            ImGui::EndCombo();
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Show FPS:");
        ImGui::TableSetColumnIndex(1);
        if (state.ui_settings) {
            bool show_fps = state.ui_settings->show_fps;
            if (ImGui::Checkbox("##show_fps", &show_fps)) {
                state.ui_settings->show_fps = show_fps;
                save_settings(state);
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

void draw_fps_overlay(GuiState &state) {
    if (!state.ui_settings || !state.ui_settings->show_fps ||
        state.mode != AppMode::Running || !state.config)
        return;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d FPS  %d VI/s",
                  static_cast<int>(std::lround(state.fps_game)),
                  static_cast<int>(std::lround(state.fps_display)));

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    const float pad = 10.0f;
    ImGui::SetNextWindowPos(
        ImVec2(vp->WorkPos.x + vp->WorkSize.x - pad, vp->WorkPos.y + pad),
        ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("##fps_overlay", nullptr, flags))
        ImGui::TextUnformatted(buf);
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

        ImGui::EndTable();
    }

    if (running)
        ImGui::TextDisabled(
            "CPU JIT applies after Stop / next Open ROM.");

    ImGui::End();
}

void draw_audio_settings(GuiState &state) {
    if (!state.show_audio_settings || !state.ui_settings)
        return;

    ImGui::SetNextWindowSize(ImVec2(400, 120), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Audio Settings", &state.show_audio_settings,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    auto &ui = *state.ui_settings;
    if (ImGui::BeginTable("##audio_settings", 2,
                          ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Volume:");
        ImGui::TableSetColumnIndex(1);
        float vol = ui.audio_volume * 100.0f;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##volume", &vol, 0.0f, 100.0f, "%.0f%%")) {
            ui.audio_volume = std::clamp(vol / 100.0f, 0.0f, 1.0f);
            Audio::set_volume(ui.audio_volume);
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            save_settings(state);

        ImGui::EndTable();
    }

    ImGui::End();
}

void draw_input_test_chip(const char *label, bool on) {
    const ImGuiStyle &style = ImGui::GetStyle();
    const ImVec4 base = style.Colors[ImGuiCol_Button];
    const ImVec4 active = style.Colors[ImGuiCol_ButtonActive];
    const ImVec4 col = on ? active
                          : ImVec4(base.x * 0.55f, base.y * 0.55f, base.z * 0.55f,
                                   base.w);
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
    ImGui::BeginDisabled();
    ImGui::Button(label, ImVec2(0.0f, 0.0f));
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

bool n64_bind_active(const Mmio::N64ControllerState &s, N64KeyBind bind) {
    using namespace Mmio;
    constexpr int kStickThresh = 40;
    switch (bind) {
    case N64KeyBind::A:
        return (s.byte1 & N64ControllerByte1::A) != 0;
    case N64KeyBind::B:
        return (s.byte1 & N64ControllerByte1::B) != 0;
    case N64KeyBind::Z:
        return (s.byte1 & N64ControllerByte1::Z) != 0;
    case N64KeyBind::Start:
        return (s.byte1 & N64ControllerByte1::START) != 0;
    case N64KeyBind::DPadUp:
        return (s.byte1 & N64ControllerByte1::DP_UP) != 0;
    case N64KeyBind::DPadDown:
        return (s.byte1 & N64ControllerByte1::DP_DOWN) != 0;
    case N64KeyBind::DPadLeft:
        return (s.byte1 & N64ControllerByte1::DP_LEFT) != 0;
    case N64KeyBind::DPadRight:
        return (s.byte1 & N64ControllerByte1::DP_RIGHT) != 0;
    case N64KeyBind::L:
        return (s.byte2 & N64ControllerByte2::L) != 0;
    case N64KeyBind::R:
        return (s.byte2 & N64ControllerByte2::R) != 0;
    case N64KeyBind::CUp:
        return (s.byte2 & N64ControllerByte2::C_UP) != 0;
    case N64KeyBind::CDown:
        return (s.byte2 & N64ControllerByte2::C_DOWN) != 0;
    case N64KeyBind::CLeft:
        return (s.byte2 & N64ControllerByte2::C_LEFT) != 0;
    case N64KeyBind::CRight:
        return (s.byte2 & N64ControllerByte2::C_RIGHT) != 0;
    case N64KeyBind::StickUp:
        return s.joy_y >= kStickThresh;
    case N64KeyBind::StickDown:
        return s.joy_y <= -kStickThresh;
    case N64KeyBind::StickLeft:
        return s.joy_x <= -kStickThresh;
    case N64KeyBind::StickRight:
        return s.joy_x >= kStickThresh;
    case N64KeyBind::Count:
        break;
    }
    return false;
}

void draw_controller_input_test(const Mmio::N64ControllerState &s) {
    ImGui::Separator();
    ImGui::TextUnformatted("Input Test");
    ImGui::TextDisabled("Press bound keys / gamepad controls to verify.");
    ImGui::Spacing();

    auto chip_row = [&](std::initializer_list<std::pair<const char *, N64KeyBind>> items) {
        bool first = true;
        for (const auto &it : items) {
            if (!first)
                ImGui::SameLine();
            first = false;
            draw_input_test_chip(it.first, n64_bind_active(s, it.second));
        }
    };

    chip_row({{"A", N64KeyBind::A},
              {"B", N64KeyBind::B},
              {"Z", N64KeyBind::Z},
              {"Start", N64KeyBind::Start},
              {"L", N64KeyBind::L},
              {"R", N64KeyBind::R}});

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("D-Pad");
    ImGui::SameLine();
    chip_row({{"Up", N64KeyBind::DPadUp},
              {"Down", N64KeyBind::DPadDown},
              {"Left", N64KeyBind::DPadLeft},
              {"Right", N64KeyBind::DPadRight}});

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("C");
    ImGui::SameLine();
    chip_row({{"Up", N64KeyBind::CUp},
              {"Down", N64KeyBind::CDown},
              {"Left", N64KeyBind::CLeft},
              {"Right", N64KeyBind::CRight}});

    ImGui::Spacing();
    const float box = ImGui::GetFrameHeight() * 4.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 fill = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_ButtonActive);
    const ImU32 cross = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    dl->AddRectFilled(origin, ImVec2(origin.x + box, origin.y + box), fill);
    dl->AddRect(origin, ImVec2(origin.x + box, origin.y + box), border);
    const ImVec2 center(origin.x + box * 0.5f, origin.y + box * 0.5f);
    dl->AddLine(ImVec2(center.x, origin.y + 4.0f),
                ImVec2(center.x, origin.y + box - 4.0f), cross);
    dl->AddLine(ImVec2(origin.x + 4.0f, center.y),
                ImVec2(origin.x + box - 4.0f, center.y), cross);
    const float nx = static_cast<float>(s.joy_x) / 127.0f;
    const float ny = static_cast<float>(-s.joy_y) / 127.0f; // screen +Y is down
    const float radius = box * 0.5f - 8.0f;
    const ImVec2 knob(center.x + nx * radius, center.y + ny * radius);
    dl->AddCircleFilled(knob, 5.0f, accent);
    ImGui::Dummy(ImVec2(box, box));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Stick  X: %d", static_cast<int>(s.joy_x));
    ImGui::Text("       Y: %d", static_cast<int>(s.joy_y));
    ImGui::EndGroup();
}

void draw_controller_settings(GuiState &state) {
    if (!state.ui_settings)
        return;

    enum class WaitKind { None, Key, Pad };
    static WaitKind waiting = WaitKind::None;
    static int waiting_index = -1;
    static uint8_t prev_keys[SDL_NUM_SCANCODES]{};

    if (!state.show_controller_settings) {
        waiting = WaitKind::None;
        waiting_index = -1;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(560, 680), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Controller Settings", &state.show_controller_settings,
                      ImGuiWindowFlags_NoCollapse)) {
        if (!state.show_controller_settings) {
            waiting = WaitKind::None;
            waiting_index = -1;
        }
        ImGui::End();
        return;
    }

    auto &ui = *state.ui_settings;
    const Mmio::N64ControllerState live = sample_controller_state();

    SDL_PumpEvents();
    const uint8_t *keys = SDL_GetKeyboardState(nullptr);

    if (waiting == WaitKind::Key && waiting_index >= 0) {
        for (int sc = SDL_SCANCODE_UNKNOWN + 1; sc < SDL_NUM_SCANCODES; ++sc) {
            if (!keys[sc] || prev_keys[sc])
                continue;
            if (sc == SDL_SCANCODE_ESCAPE) {
                waiting = WaitKind::None;
                waiting_index = -1;
                break;
            }
            for (int i = 0; i < kN64KeyBindCount; ++i) {
                if (i != waiting_index && ui.key_binds[i] == sc)
                    ui.key_binds[i] = SDL_SCANCODE_UNKNOWN;
            }
            ui.key_binds[waiting_index] = sc;
            set_key_binds(ui.key_binds);
            save_settings(state);
            waiting = WaitKind::None;
            waiting_index = -1;
            break;
        }
    } else if (waiting == WaitKind::Pad && waiting_index >= 0) {
        if (keys[SDL_SCANCODE_ESCAPE] && !prev_keys[SDL_SCANCODE_ESCAPE]) {
            waiting = WaitKind::None;
            waiting_index = -1;
        } else {
            const PadBind edge = poll_pad_bind_edge();
            if (edge.kind != PadBindKind::None) {
                for (int i = 0; i < kN64KeyBindCount; ++i) {
                    if (i != waiting_index && ui.pad_binds[i] == edge)
                        ui.pad_binds[i] = {};
                }
                ui.pad_binds[waiting_index] = edge;
                set_pad_binds(ui.pad_binds);
                save_settings(state);
                waiting = WaitKind::None;
                waiting_index = -1;
            }
        }
    }
    std::memcpy(prev_keys, keys, sizeof(prev_keys));

    const ImGuiStyle &style = ImGui::GetStyle();
    const float bind_btn_w =
        std::max(ImGui::CalcTextSize("Press key... (Esc)").x,
                 ImGui::CalcTextSize("Press button... (Esc)").x) +
        style.FramePadding.x * 2.0f;

    if (ImGui::BeginTable("##controller_binds", 3,
                          ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_NoSavedSettings |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Keyboard", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Gamepad", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (int i = 0; i < kN64KeyBindCount; ++i) {
            const auto bind = static_cast<N64KeyBind>(i);
            const bool active = n64_bind_active(live, bind);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Text,
                                     ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            ImGui::TextUnformatted(n64_key_bind_label(bind));
            if (active)
                ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(1);
            const char *key_name = "-";
            if (waiting == WaitKind::Key && waiting_index == i) {
                key_name = "Press key... (Esc)";
            } else if (ui.key_binds[i] > SDL_SCANCODE_UNKNOWN) {
                const char *n = SDL_GetScancodeName(
                    static_cast<SDL_Scancode>(ui.key_binds[i]));
                if (n && n[0])
                    key_name = n;
            }
            ImGui::PushID(i * 2);
            if (ImGui::Button(key_name, ImVec2(bind_btn_w, 0.0f))) {
                waiting = WaitKind::Key;
                waiting_index = i;
                std::memcpy(prev_keys, keys, sizeof(prev_keys));
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            char pad_label[64];
            const char *pad_name = pad_bind_label(ui.pad_binds[i], pad_label,
                                                  sizeof(pad_label));
            if (waiting == WaitKind::Pad && waiting_index == i)
                pad_name = "Press button... (Esc)";
            ImGui::PushID(i * 2 + 1);
            if (ImGui::Button(pad_name, ImVec2(bind_btn_w, 0.0f))) {
                waiting = WaitKind::Pad;
                waiting_index = i;
                // Warm edge detector so currently-held inputs are ignored.
                (void)poll_pad_bind_edge();
                std::memcpy(prev_keys, keys, sizeof(prev_keys));
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset to defaults")) {
        default_key_binds(ui.key_binds);
        default_pad_binds(ui.pad_binds);
        set_key_binds(ui.key_binds);
        set_pad_binds(ui.pad_binds);
        waiting = WaitKind::None;
        waiting_index = -1;
        save_settings(state);
    }

    draw_controller_input_test(live);

    ImGui::End();
}

void draw_folder_open_icon(ImDrawList *dl, ImVec2 center, float size) {
    // Flat folder + green add badge (Dolphin-style empty state).
    const float w = size;
    const float h = size * 0.78f;
    const ImVec2 o(center.x - w * 0.5f, center.y - h * 0.55f);

    const ImU32 folder = IM_COL32(232, 176, 64, 255);
    const ImU32 folder_dark = IM_COL32(196, 140, 40, 255);
    const ImU32 badge = IM_COL32(76, 175, 80, 255);
    const ImU32 badge_plus = IM_COL32(255, 255, 255, 255);

    const float tab_w = w * 0.38f;
    const float tab_h = h * 0.18f;
    const float body_y = o.y + tab_h * 0.55f;
    dl->AddRectFilled(ImVec2(o.x, o.y), ImVec2(o.x + tab_w, body_y + 2.0f),
                      folder_dark, 3.0f);
    dl->AddRectFilled(ImVec2(o.x, body_y), ImVec2(o.x + w, o.y + h), folder,
                      5.0f);

    const float r = size * 0.22f;
    const ImVec2 badge_c(o.x + w - r * 0.15f, o.y + h - r * 0.15f);
    dl->AddCircleFilled(badge_c, r, badge, 24);
    const float arm = r * 0.45f;
    const float thick = std::max(2.0f, r * 0.22f);
    dl->AddRectFilled(ImVec2(badge_c.x - arm, badge_c.y - thick * 0.5f),
                      ImVec2(badge_c.x + arm, badge_c.y + thick * 0.5f),
                      badge_plus, 1.0f);
    dl->AddRectFilled(ImVec2(badge_c.x - thick * 0.5f, badge_c.y - arm),
                      ImVec2(badge_c.x + thick * 0.5f, badge_c.y + arm),
                      badge_plus, 1.0f);
}

void draw_home(GuiState &state) {
    if (state.mode != AppMode::Menu)
        return;

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (!ImGui::Begin("##home", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    if (ImGui::IsWindowHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        open_rom_dialog(state);

    const ImVec2 center(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                        vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    const float icon_size = std::clamp(vp->WorkSize.y * 0.14f, 64.0f, 112.0f);
    draw_folder_open_icon(ImGui::GetWindowDrawList(),
                          ImVec2(center.x, center.y - icon_size * 0.15f),
                          icon_size);

    const char *hint = "Double-click to open a ROM";
    ImFont *font = ImGui::GetFont();
    const float font_size = ImGui::GetStyle().FontSizeBase * 2.25f;
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, hint);
    ImGui::GetWindowDrawList()->AddText(
        font, font_size,
        ImVec2(center.x - text_size.x * 0.5f, center.y + icon_size * 0.55f),
        ImGui::GetColorU32(ImGuiCol_Text), hint);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
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
    ImGui::TextUnformatted(kAppDisplayName);
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
    ImGui::TextLinkOpenURL(kAppGithubDisplay, kAppGithubUrl);

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::PushFont(nullptr, style.FontSizeBase * 0.85f);
    ImGui::TextDisabled("%s", kAppCopyright);
    ImGui::PopFont();

    ImGui::End();
}

} // namespace

void gui_draw(GuiState &state) {
    static bool menu_open = false;
    const bool fullscreen = window_is_fullscreen(current_window(state));
    const bool hide_menu_bar =
        state.ui_settings && state.ui_settings->hide_menu_bar;
    const bool auto_hide =
        hide_menu_bar && state.mode == AppMode::Running;
    bool any_menu_open = false;
    auto begin_menu = [&any_menu_open](const char *label) {
        const bool open = ImGui::BeginMenu(label);
        any_menu_open |= open;
        return open;
    };

    const bool show_menu_bar = menu_bar_visible(auto_hide, menu_open);
    state.menu_bar_active = menu_open || (auto_hide && show_menu_bar);

    if (show_menu_bar && ImGui::BeginMainMenuBar()) {
        if (begin_menu("File")) {
            if (ImGui::MenuItem("Open ROM...", "Ctrl+O"))
                open_rom_dialog(state);
            draw_recents_menu(state);
            if (ImGui::MenuItem(kMenuOpenFolder))
                open_settings_dir();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                state.request_quit = true;
            ImGui::EndMenu();
        }

        if (begin_menu("Emulation")) {
            const bool running = state.mode == AppMode::Running;
            if (ImGui::MenuItem("Stop", nullptr, false, running))
                state.request_stop = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Video Settings"))
                state.show_video_settings = true;
            if (ImGui::MenuItem("Emulation Settings"))
                state.show_emu_settings = true;
            if (ImGui::MenuItem("Audio Settings"))
                state.show_audio_settings = true;
            if (ImGui::MenuItem("Controller Settings"))
                state.show_controller_settings = true;
            ImGui::EndMenu();
        }

        if (begin_menu("View")) {
            if (ImGui::MenuItem("Fullscreen", "F11", fullscreen))
                set_fullscreen(state, true);
            if (ImGui::MenuItem("Window", nullptr, !fullscreen))
                set_fullscreen(state, false);
            ImGui::Separator();
            if (state.ui_settings) {
                bool hide = state.ui_settings->hide_menu_bar;
                if (ImGui::MenuItem("Hide Menu Bar", nullptr, hide)) {
                    state.ui_settings->hide_menu_bar = !hide;
                    save_settings(state);
                }
            }
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

        if (begin_menu("Help")) {
            if (ImGui::MenuItem(kMenuAbout))
                state.show_about = true;
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
    menu_open = any_menu_open;
    state.menu_bar_active = state.menu_bar_active || any_menu_open;

    draw_home(state);
    draw_video_settings(state);
    draw_emu_settings(state);
    draw_audio_settings(state);
    draw_controller_settings(state);
    draw_about(state);
    draw_fps_overlay(state);
}

} // namespace Ui
} // namespace N64

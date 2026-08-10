#include "ui/config_toml.h"
#include "ui/input_sdl.h"
#include "utils/log.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <toml++/toml.hpp>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace N64 {
namespace Ui {

namespace {

namespace fs = std::filesystem;

constexpr const char *kSettingsFileName = "n64-emu.toml";

std::string join_dir_file(const std::string &dir, const char *file) {
    if (dir.empty())
        return file;
    if (dir.back() == '/' || dir.back() == '\\')
        return dir + file;
    return dir + '/' + file;
}

// SDL_GetPrefPath always makes org/app/; we only want a single n64-emu folder.
// Windows: %APPDATA%/n64-emu/
// Linux:   ~/.local/share/n64-emu/
// macOS:   ~/Library/Application Support/n64-emu/
std::string pref_settings_dir() {
    if (char *pref = SDL_GetPrefPath("n64-emu", "n64-emu")) {
        fs::path nested(pref); // .../n64-emu/n64-emu/
        SDL_free(pref);
        fs::path dir = nested.parent_path(); // .../n64-emu/
        std::error_code ec;
        fs::create_directories(dir, ec);
        // Drop the empty inner directory SDL created, if unused.
        fs::remove(nested, ec);
        return dir.string();
    }
    return {};
}

std::string pref_settings_path() {
    const std::string dir = pref_settings_dir();
    if (dir.empty())
        return {};
    return join_dir_file(dir, kSettingsFileName);
}

// Older locations we may migrate from.
std::vector<std::string> legacy_settings_paths() {
    std::vector<std::string> out;
    if (char *pref = SDL_GetPrefPath("n64-emu", "n64-emu")) {
        // Previous double-nested path.
        out.push_back(join_dir_file(pref, kSettingsFileName));
        SDL_free(pref);
    }
    if (char *base = SDL_GetBasePath()) {
        out.push_back(join_dir_file(base, kSettingsFileName));
        SDL_free(base);
    }
    std::error_code ec;
    auto cwd = fs::current_path(ec);
    if (!ec)
        out.push_back((cwd / kSettingsFileName).string());
    return out;
}

bool file_exists(const std::string &path) {
    std::error_code ec;
    return !path.empty() && fs::is_regular_file(path, ec);
}

bool migrate_legacy_settings(const std::string &pref_path) {
    if (file_exists(pref_path))
        return false;

    for (const std::string &legacy : legacy_settings_paths()) {
        if (legacy.empty() || legacy == pref_path || !file_exists(legacy))
            continue;

        std::error_code ec;
        fs::create_directories(fs::path(pref_path).parent_path(), ec);
        fs::copy_file(legacy, pref_path, fs::copy_options::none, ec);
        if (ec) {
            Utils::warn("Failed to migrate settings {} -> {}: {}", legacy,
                        pref_path, ec.message());
            continue;
        }
        fs::remove(legacy, ec);
        // Also drop an empty nested pref dir left from the old layout.
        fs::remove(fs::path(legacy).parent_path(), ec);
        Utils::info("Migrated settings from {} to {}", legacy, pref_path);
        return true;
    }
    return false;
}

bool parse_toml_file(const std::string &path, N64System::Config &config,
                     UiSettings &ui) {
    try {
        auto tbl = toml::parse_file(path);
        if (auto *video = tbl["video"].as_table()) {
            if (auto v = (*video)["upscale"].value<int64_t>()) {
                const unsigned u = static_cast<unsigned>(*v);
                if (u == 1 || u == 2 || u == 4 || u == 8)
                    config.upscale = u;
            }
            if (auto v = (*video)["frame_interp"].value<bool>())
                config.frame_interp = *v;
            if (auto v = (*video)["frame_interp_mode"].value<std::string>()) {
                if (*v == "extrapolate" || *v == "onesided" || *v == "one-sided")
                    config.frame_interp_mode =
                        N64System::FrameInterpMode::Extrapolate;
                else
                    config.frame_interp_mode =
                        N64System::FrameInterpMode::Bidirectional;
            }
            if (auto v = (*video)["vulkan_device"].value<std::string>())
                config.vulkan_device = *v;
        }
        if (auto *cpu = tbl["cpu"].as_table()) {
            if (auto v = (*cpu)["jit"].value<bool>()) {
                config.cpu_backend = *v ? N64System::CpuBackend::Jit
                                        : N64System::CpuBackend::Interpreter;
            }
            if (auto v = (*cpu)["rsp_thread"].value<bool>())
                config.rsp_thread = *v;
        }
        if (auto *u = tbl["ui"].as_table()) {
            if (auto v = (*u)["last_rom_dir"].value<std::string>())
                ui.last_rom_dir = *v;
            if (auto v = (*u)["theme"].value<std::string>()) {
                if (*v == "light")
                    ui.theme = UiTheme::Light;
                else if (*v == "dark")
                    ui.theme = UiTheme::Dark;
            }
            if (auto v = (*u)["show_fps"].value<bool>())
                ui.show_fps = *v;
            if (auto v = (*u)["hide_menu_bar"].value<bool>())
                ui.hide_menu_bar = *v;
        }
        if (auto *audio = tbl["audio"].as_table()) {
            if (auto v = (*audio)["volume"].value<double>())
                ui.audio_volume =
                    std::clamp(static_cast<float>(*v), 0.0f, 1.0f);
        }
        if (auto *input = tbl["input"].as_table()) {
            for (int i = 0; i < kN64KeyBindCount; ++i) {
                const auto bind = static_cast<N64KeyBind>(i);
                if (auto v =
                        (*input)[n64_key_bind_toml_key(bind)].value<std::string>()) {
                    const SDL_Scancode sc = SDL_GetScancodeFromName(v->c_str());
                    if (sc != SDL_SCANCODE_UNKNOWN)
                        ui.key_binds[i] = static_cast<int>(sc);
                }
            }
        }
        if (auto *pad = tbl["input_pad"].as_table()) {
            for (int i = 0; i < kN64KeyBindCount; ++i) {
                const auto bind = static_cast<N64KeyBind>(i);
                if (auto v =
                        (*pad)[n64_key_bind_toml_key(bind)].value<std::string>()) {
                    PadBind pb{};
                    if (pad_bind_from_string(v->c_str(), pb))
                        ui.pad_binds[i] = pb;
                }
            }
        }
        Utils::info("Loaded settings from {}", path);
        return true;
    } catch (const toml::parse_error &) {
        return false;
    } catch (const std::exception &) {
        return false;
    }
}

} // namespace

std::string settings_toml_path() {
    if (std::string pref = pref_settings_path(); !pref.empty())
        return pref;
    const auto legacy = legacy_settings_paths();
    return legacy.empty() ? kSettingsFileName : legacy.back();
}

std::string settings_dir_path() {
    const fs::path toml = settings_toml_path();
    fs::path dir = toml.has_parent_path() ? toml.parent_path() : fs::path(".");
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

bool open_settings_dir() {
    const std::string dir = settings_dir_path();
    if (dir.empty())
        return false;
#ifdef _WIN32
    const int n = MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, nullptr, 0);
    if (n <= 1)
        return false;
    std::wstring wdir(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, wdir.data(), n);
    const INT_PTR rc =
        reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wdir.c_str(),
                                                nullptr, nullptr, SW_SHOWNORMAL));
    if (rc <= 32) {
        Utils::warn("Failed to open settings folder {}", dir);
        return false;
    }
    return true;
#else
    const std::string url = std::string("file://") + dir;
    if (SDL_OpenURL(url.c_str()) != 0) {
        Utils::warn("Failed to open settings folder {}: {}", dir, SDL_GetError());
        return false;
    }
    return true;
#endif
}

bool load_toml(N64System::Config &config, UiSettings &ui) {
    const std::string path = settings_toml_path();
    migrate_legacy_settings(path);
    if (!file_exists(path))
        return false;
    return parse_toml_file(path, config, ui);
}

bool save_toml(const N64System::Config &config, const UiSettings &ui) {
    toml::table video;
    video.insert_or_assign("upscale", static_cast<int64_t>(config.upscale));
    video.insert_or_assign("frame_interp", config.frame_interp);
    video.insert_or_assign(
        "frame_interp_mode",
        config.frame_interp_mode == N64System::FrameInterpMode::Extrapolate
            ? "extrapolate"
            : "bidirectional");
    video.insert_or_assign("vulkan_device", config.vulkan_device);

    toml::table cpu;
    cpu.insert_or_assign("jit",
                         config.cpu_backend == N64System::CpuBackend::Jit);
    cpu.insert_or_assign("rsp_thread", config.rsp_thread);

    toml::table ui_tbl;
    ui_tbl.insert_or_assign("last_rom_dir", ui.last_rom_dir);
    ui_tbl.insert_or_assign(
        "theme", ui.theme == UiTheme::Light ? "light" : "dark");
    ui_tbl.insert_or_assign("show_fps", ui.show_fps);
    ui_tbl.insert_or_assign("hide_menu_bar", ui.hide_menu_bar);

    toml::table audio;
    audio.insert_or_assign("volume", static_cast<double>(ui.audio_volume));

    toml::table input;
    for (int i = 0; i < kN64KeyBindCount; ++i) {
        const auto bind = static_cast<N64KeyBind>(i);
        const char *name =
            SDL_GetScancodeName(static_cast<SDL_Scancode>(ui.key_binds[i]));
        input.insert_or_assign(n64_key_bind_toml_key(bind),
                               name && name[0] ? name : "Unknown");
    }

    toml::table input_pad;
    for (int i = 0; i < kN64KeyBindCount; ++i) {
        const auto bind = static_cast<N64KeyBind>(i);
        char buf[64];
        if (!pad_bind_to_string(ui.pad_binds[i], buf, sizeof(buf)))
            std::snprintf(buf, sizeof(buf), "none");
        input_pad.insert_or_assign(n64_key_bind_toml_key(bind), buf);
    }

    toml::table tbl;
    tbl.insert_or_assign("video", std::move(video));
    tbl.insert_or_assign("cpu", std::move(cpu));
    tbl.insert_or_assign("ui", std::move(ui_tbl));
    tbl.insert_or_assign("audio", std::move(audio));
    tbl.insert_or_assign("input", std::move(input));
    tbl.insert_or_assign("input_pad", std::move(input_pad));

    const std::string path = settings_toml_path();
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        Utils::warn("Failed to write {}", path);
        return false;
    }
    out << tbl;
    return true;
}

} // namespace Ui
} // namespace N64

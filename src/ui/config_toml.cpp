#include "ui/config_toml.h"
#include "utils/log.h"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <toml++/toml.hpp>

namespace N64 {
namespace Ui {

std::string settings_toml_path() {
    if (SDL_WasInit(SDL_INIT_VIDEO) || SDL_WasInit(SDL_INIT_EVENTS)) {
        if (char *base = SDL_GetBasePath()) {
            std::string path = std::string(base) + "n64-emu.toml";
            SDL_free(base);
            return path;
        }
    }
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (!ec)
        return (cwd / "n64-emu.toml").string();
    return "n64-emu.toml";
}

bool load_toml(N64System::Config &config, UiSettings &ui) {
    const std::string path = settings_toml_path();
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
        }
        Utils::info("Loaded settings from {}", path);
        return true;
    } catch (const toml::parse_error &) {
        return false;
    } catch (const std::exception &) {
        return false;
    }
}

bool save_toml(const N64System::Config &config, const UiSettings &ui) {
    toml::table video;
    video.insert_or_assign("upscale", static_cast<int64_t>(config.upscale));
    video.insert_or_assign("frame_interp", config.frame_interp);

    toml::table cpu;
    cpu.insert_or_assign("jit",
                         config.cpu_backend == N64System::CpuBackend::Jit);
    cpu.insert_or_assign("rsp_thread", config.rsp_thread);

    toml::table ui_tbl;
    ui_tbl.insert_or_assign("last_rom_dir", ui.last_rom_dir);
    ui_tbl.insert_or_assign(
        "theme", ui.theme == UiTheme::Light ? "light" : "dark");

    toml::table tbl;
    tbl.insert_or_assign("video", std::move(video));
    tbl.insert_or_assign("cpu", std::move(cpu));
    tbl.insert_or_assign("ui", std::move(ui_tbl));

    const std::string path = settings_toml_path();
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

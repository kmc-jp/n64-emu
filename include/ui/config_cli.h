#pragma once

#include "n64_system/config.h"
#include "ui/input_sdl.h"
#include <string>

namespace N64 {
namespace Ui {

enum class UiTheme { Light, Dark };

struct UiSettings {
    std::string last_rom_dir{};
    UiTheme theme{UiTheme::Dark};
    // Linear output gain in [0, 1].
    float audio_volume{1.0f};
    bool show_fps{false};
    // SDL_Scancode values indexed by N64KeyBind.
    int key_binds[kN64KeyBindCount]{};
    // XInput / SDL GameController bindings indexed by N64KeyBind.
    PadBind pad_binds[kN64KeyBindCount]{};

    UiSettings() {
        default_key_binds(key_binds);
        default_pad_binds(pad_binds);
    }
};

// Apply CLI flags onto an existing config (defaults / TOML already applied).
// Returns false on parse error. Windowed mode allows argc==1 (no ROM).
// Headless/test still require a ROM path.
bool apply_command_line(N64System::Config &config, int argc, char *argv[]);

} // namespace Ui
} // namespace N64

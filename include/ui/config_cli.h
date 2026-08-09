#pragma once

#include "n64_system/config.h"
#include <string>

namespace N64 {
namespace Ui {

enum class UiTheme { Light, Dark };

struct UiSettings {
    std::string last_rom_dir{};
    UiTheme theme{UiTheme::Dark};
};

// Apply CLI flags onto an existing config (defaults / TOML already applied).
// Returns false on parse error. Windowed mode allows argc==1 (no ROM).
// Headless/test still require a ROM path.
bool apply_command_line(N64System::Config &config, int argc, char *argv[]);

} // namespace Ui
} // namespace N64

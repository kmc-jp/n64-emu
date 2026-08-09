#pragma once

#include "n64_system/config.h"
#include "ui/config_cli.h"
#include <string>

namespace N64 {
namespace Ui {

std::string settings_toml_path();

// Load n64-emu.toml into config + ui settings. Missing file is OK.
bool load_toml(N64System::Config &config, UiSettings &ui);

// Write current settings.
bool save_toml(const N64System::Config &config, const UiSettings &ui);

} // namespace Ui
} // namespace N64

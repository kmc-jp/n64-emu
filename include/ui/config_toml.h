#pragma once

#include "n64_system/config.h"
#include "ui/config_cli.h"
#include <string>

namespace N64 {
namespace Ui {

// OS app-data path, e.g. %APPDATA%/n64-emu/n64-emu.toml
std::string settings_toml_path();

// Directory containing settings_toml_path() (created if missing).
std::string settings_dir_path();

// Open settings_dir_path() in the OS file manager.
bool open_settings_dir();

// Load settings from the pref path. Migrates a legacy next-to-exe file once.
// Missing file is OK.
bool load_toml(N64System::Config &config, UiSettings &ui);

// Write current settings to the pref path.
bool save_toml(const N64System::Config &config, const UiSettings &ui);

} // namespace Ui
} // namespace N64

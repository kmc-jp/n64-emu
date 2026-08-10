#pragma once

#include <string>

namespace N64 {
namespace Ui {

struct GuiState;

// True for common N64 ROM extensions (.z64 / .n64 / .v64), case-insensitive.
bool is_n64_rom_path(const std::string &path);

// Open a ROM by path: updates rom_filepath, last_rom_dir, saves TOML, and
// sets request_start (and request_stop if already running).
bool open_rom_file(GuiState &state, const std::string &path);

// Native Open File dialog. On success calls open_rom_file.
bool open_rom_dialog(GuiState &state);

} // namespace Ui
} // namespace N64

#pragma once

namespace N64 {
namespace Ui {

struct GuiState;

// Native Open File dialog. On success updates rom_filepath, last_rom_dir,
// saves TOML, and sets request_start (and request_stop if already running).
bool open_rom_dialog(GuiState &state);

} // namespace Ui
} // namespace N64

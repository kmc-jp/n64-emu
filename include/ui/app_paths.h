#pragma once

#include <string>

namespace N64 {
namespace Ui {

// Single app-data folder (e.g. %APPDATA%/kamo64 or ~/.local/share/kamo64).
// SDL_GetPrefPath(org, app) creates org/app/; we peel that to one level.
// Creates the directory. Moves leftover files out of the empty inner SDL dir.
std::string app_data_dir();

} // namespace Ui
} // namespace N64

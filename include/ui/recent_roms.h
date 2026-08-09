#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace N64 {
namespace Ui {

constexpr std::size_t kMaxRecentRoms = 10;

// Path to recent_roms.txt in the settings directory.
std::string recent_roms_path();

std::vector<std::string> load_recent_roms();
bool save_recent_roms(const std::vector<std::string> &paths);

// Move path to front (dedupe), cap at kMaxRecentRoms, and persist.
void remember_recent_rom(std::vector<std::string> &paths, const std::string &path);

} // namespace Ui
} // namespace N64

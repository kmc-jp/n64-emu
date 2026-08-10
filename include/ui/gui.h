#pragma once

#include "n64_system/config.h"
#include "ui/config_cli.h"
#include "wsi.hpp"
#include <string>
#include <vector>

namespace N64 {
namespace Ui {

enum class AppMode { Menu, Running };

struct GuiState {
    AppMode mode{AppMode::Menu};
    bool show_video_settings{false};
    bool show_emu_settings{false};
    bool show_audio_settings{false};
    bool show_controller_settings{false};
    bool show_about{false};
    bool request_start{false};
    bool request_stop{false};
    bool request_quit{false};
    N64System::Config *config{nullptr};
    UiSettings *ui_settings{nullptr};
    Vulkan::WSI *wsi{nullptr};
    uint8_t *rdram{nullptr};
    // vulkan_device value that was used for this process's WSI init.
    std::string applied_vulkan_device{};
    std::vector<std::string> recent_roms{};
};

void gui_draw(GuiState &state);

} // namespace Ui
} // namespace N64

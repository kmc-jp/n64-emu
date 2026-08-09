#pragma once

#include "n64_system/config.h"
#include "ui/config_cli.h"

struct SDL_Window;

namespace N64 {
namespace Ui {

// GUI frontend.
// Menu lives on `window`; emulation presents to a separate `game_window`.
class App {
  private:
    N64System::Config config;
    UiSettings ui_settings;
    ::SDL_Window *window;
    ::SDL_Window *game_window;

  public:
    App(N64System::Config &config, UiSettings &ui_settings);
    ~App();
    void run();
};

} // namespace Ui
} // namespace N64

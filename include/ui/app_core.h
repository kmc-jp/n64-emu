#pragma once

#include "n64_system/config.h"

struct SDL_Window;

namespace N64 {
namespace Ui {

// CLI / windowed frontend.
class AppCore {
  public:
    explicit AppCore(N64System::Config &config);
    ~AppCore();
    void run();

  private:
    N64System::Config config;
    ::SDL_Window *window;

    void run_headless();
    void run_windowed();
};

} // namespace Ui
} // namespace N64

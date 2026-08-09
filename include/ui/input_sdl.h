#pragma once

namespace N64 {
namespace Ui {

// Map SDL keyboard state into core controller channel 0.
// When capture_keyboard is true (ImGui wants keys), inject an empty state.
void poll_and_inject_controller(bool capture_keyboard);

} // namespace Ui
} // namespace N64

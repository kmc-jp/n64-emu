#include "mmio/controller_input.h"
#include <array>
#include <mutex>

namespace N64 {
namespace Input {

namespace {
std::mutex g_mu;
std::array<Mmio::N64ControllerState, kMaxControllers> g_states{};
} // namespace

void set_controller_state(int channel, const Mmio::N64ControllerState &state) {
    if (channel < 0 || channel >= kMaxControllers)
        return;
    std::lock_guard lock(g_mu);
    g_states[static_cast<size_t>(channel)] = state;
}

Mmio::N64ControllerState get_controller_state(int channel) {
    if (channel < 0 || channel >= kMaxControllers)
        return {};
    std::lock_guard lock(g_mu);
    return g_states[static_cast<size_t>(channel)];
}

} // namespace Input
} // namespace N64

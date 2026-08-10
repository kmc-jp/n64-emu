#include "mmio/controller_input.h"
#include <array>
#include <mutex>

namespace N64 {
namespace Input {

namespace {
std::mutex g_mu;
std::array<Mmio::N64ControllerState, kMaxControllers> g_states{};
HostPollFn g_host_poll = nullptr;
} // namespace

void set_host_poll(HostPollFn fn) { g_host_poll = fn; }

void poll_host() {
    if (g_host_poll)
        g_host_poll();
}

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

#pragma once

#include "mmio/pif.h"
#include <cstddef>
#include <cstdint>

union SDL_Event;

namespace N64 {
namespace Ui {

// N64 controller actions that can be bound to keyboard / gamepad.
enum class N64KeyBind : int {
    A = 0,
    B,
    Z,
    Start,
    DPadUp,
    DPadDown,
    DPadLeft,
    DPadRight,
    L,
    R,
    CUp,
    CDown,
    CLeft,
    CRight,
    StickUp,
    StickDown,
    StickLeft,
    StickRight,
    Count
};

inline constexpr int kN64KeyBindCount = static_cast<int>(N64KeyBind::Count);

enum class PadBindKind : uint8_t {
    None = 0,
    Button,  // SDL_GameControllerButton
    AxisPos, // axis >= +deadzone
    AxisNeg, // axis <= -deadzone
};

struct PadBind {
    PadBindKind kind{PadBindKind::None};
    int8_t id{0}; // button or axis index

    bool operator==(const PadBind &o) const {
        return kind == o.kind && id == o.id;
    }
    bool operator!=(const PadBind &o) const { return !(*this == o); }
};

// Short UI label ("A", "C-Up", "Stick Left", ...).
const char *n64_key_bind_label(N64KeyBind bind);

// Stable toml key ("a", "c_up", "stick_left", ...).
const char *n64_key_bind_toml_key(N64KeyBind bind);

void default_key_binds(int out[kN64KeyBindCount]);
void default_pad_binds(PadBind out[kN64KeyBindCount]);

// Human-readable pad bind ("A", "Left Stick Left", "LT", ...).
const char *pad_bind_label(PadBind bind, char *buf, size_t buf_size);
// Serialize / parse for toml ("a", "+leftx", "-lefty", ...).
bool pad_bind_to_string(PadBind bind, char *buf, size_t buf_size);
bool pad_bind_from_string(const char *s, PadBind &out);

void set_key_binds(const int binds[kN64KeyBindCount]);
void get_key_binds(int out[kN64KeyBindCount]);
void set_pad_binds(const PadBind binds[kN64KeyBindCount]);
void get_pad_binds(PadBind out[kN64KeyBindCount]);

// Open first XInput/SDL game controller; handle hotplug via input_handle_event.
void input_init();
void input_shutdown();
void input_handle_event(const SDL_Event &e);

// Map keyboard + gamepad into core controller channel 0.
// When capture_keyboard is true (ImGui wants keys), inject an empty state.
void poll_and_inject_controller(bool capture_keyboard);

// Sample mapped Player 1 state without respecting ImGui keyboard capture.
// Used by Controller Settings input test.
Mmio::N64ControllerState sample_controller_state();

// For rebinding UI: newly pressed pad control, or None if none.
PadBind poll_pad_bind_edge();

} // namespace Ui
} // namespace N64

#include "ui/input_sdl.h"
#include "mmio/controller_input.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace N64 {
namespace Ui {

namespace {

constexpr Sint16 kAxisDeadzone = 10000;     // ~30% of 32767
constexpr Sint16 kAxisBindThreshold = 20000; // rebinding threshold

int g_key_binds[kN64KeyBindCount];
PadBind g_pad_binds[kN64KeyBindCount];
bool g_binds_ready = false;

SDL_GameController *g_pad = nullptr;
int g_pad_instance_id = -1;

bool g_prev_buttons[SDL_CONTROLLER_BUTTON_MAX]{};
bool g_prev_axes_active[SDL_CONTROLLER_AXIS_MAX * 2]{}; // pos/neg per axis

void ensure_binds() {
    if (g_binds_ready)
        return;
    default_key_binds(g_key_binds);
    default_pad_binds(g_pad_binds);
    g_binds_ready = true;
}

bool key_down(const uint8_t *state, N64KeyBind bind) {
    const int sc = g_key_binds[static_cast<int>(bind)];
    return sc > SDL_SCANCODE_UNKNOWN && sc < SDL_NUM_SCANCODES && state[sc];
}

void close_pad() {
    if (g_pad) {
        SDL_GameControllerClose(g_pad);
        g_pad = nullptr;
        g_pad_instance_id = -1;
    }
}

void open_pad_index(int index) {
    if (!SDL_IsGameController(index))
        return;
    SDL_GameController *pad = SDL_GameControllerOpen(index);
    if (!pad)
        return;
    close_pad();
    g_pad = pad;
    SDL_Joystick *js = SDL_GameControllerGetJoystick(pad);
    g_pad_instance_id = js ? SDL_JoystickInstanceID(js) : -1;
    std::memset(g_prev_buttons, 0, sizeof(g_prev_buttons));
    std::memset(g_prev_axes_active, 0, sizeof(g_prev_axes_active));
}

void try_open_first_pad() {
    if (g_pad)
        return;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            open_pad_index(i);
            return;
        }
    }
}

bool pad_button_down(SDL_GameControllerButton button) {
    return g_pad && button != SDL_CONTROLLER_BUTTON_INVALID &&
           SDL_GameControllerGetButton(g_pad, button) != 0;
}

Sint16 pad_axis_value(SDL_GameControllerAxis axis) {
    if (!g_pad || axis == SDL_CONTROLLER_AXIS_INVALID)
        return 0;
    return SDL_GameControllerGetAxis(g_pad, axis);
}

bool pad_bind_active(PadBind bind) {
    if (!g_pad || bind.kind == PadBindKind::None)
        return false;
    switch (bind.kind) {
    case PadBindKind::Button:
        return pad_button_down(
            static_cast<SDL_GameControllerButton>(bind.id));
    case PadBindKind::AxisPos:
        return pad_axis_value(static_cast<SDL_GameControllerAxis>(bind.id)) >=
               kAxisDeadzone;
    case PadBindKind::AxisNeg:
        return pad_axis_value(static_cast<SDL_GameControllerAxis>(bind.id)) <=
               -kAxisDeadzone;
    case PadBindKind::None:
        break;
    }
    return false;
}

// Map full axis -32768..32767 -> -127..127 with deadzone.
int8_t scale_axis_to_stick(Sint16 v) {
    if (v > -kAxisDeadzone && v < kAxisDeadzone)
        return 0;
    const int sign = v < 0 ? -1 : 1;
    const int abs_v = v == -32768 ? 32767 : std::abs(static_cast<int>(v));
    const int adjusted =
        std::max(0, abs_v - static_cast<int>(kAxisDeadzone));
    const int span = 32767 - static_cast<int>(kAxisDeadzone);
    const int scaled = (adjusted * 127 + span / 2) / span;
    return static_cast<int8_t>(sign * std::min(scaled, 127));
}

bool stick_bind_is_left_axis(N64KeyBind bind, PadBind pad) {
    if (pad.kind != PadBindKind::AxisPos && pad.kind != PadBindKind::AxisNeg)
        return false;
    const auto axis = static_cast<SDL_GameControllerAxis>(pad.id);
    if (bind == N64KeyBind::StickLeft || bind == N64KeyBind::StickRight)
        return axis == SDL_CONTROLLER_AXIS_LEFTX;
    if (bind == N64KeyBind::StickUp || bind == N64KeyBind::StickDown)
        return axis == SDL_CONTROLLER_AXIS_LEFTY;
    return false;
}

bool use_analog_left_stick() {
    return stick_bind_is_left_axis(
               N64KeyBind::StickLeft,
               g_pad_binds[static_cast<int>(N64KeyBind::StickLeft)]) ||
           stick_bind_is_left_axis(
               N64KeyBind::StickRight,
               g_pad_binds[static_cast<int>(N64KeyBind::StickRight)]) ||
           stick_bind_is_left_axis(
               N64KeyBind::StickUp,
               g_pad_binds[static_cast<int>(N64KeyBind::StickUp)]) ||
           stick_bind_is_left_axis(
               N64KeyBind::StickDown,
               g_pad_binds[static_cast<int>(N64KeyBind::StickDown)]);
}

const char *friendly_button_name(SDL_GameControllerButton b) {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_A:
        return "A";
    case SDL_CONTROLLER_BUTTON_B:
        return "B";
    case SDL_CONTROLLER_BUTTON_X:
        return "X";
    case SDL_CONTROLLER_BUTTON_Y:
        return "Y";
    case SDL_CONTROLLER_BUTTON_BACK:
        return "Back";
    case SDL_CONTROLLER_BUTTON_GUIDE:
        return "Guide";
    case SDL_CONTROLLER_BUTTON_START:
        return "Start";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        return "LS";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        return "RS";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        return "LB";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        return "RB";
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return "D-Pad Up";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return "D-Pad Down";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return "D-Pad Left";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return "D-Pad Right";
    default:
        break;
    }
    return nullptr;
}

const char *friendly_axis_name(SDL_GameControllerAxis axis, bool positive) {
    switch (axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        return positive ? "Left Stick Right" : "Left Stick Left";
    case SDL_CONTROLLER_AXIS_LEFTY:
        return positive ? "Left Stick Down" : "Left Stick Up";
    case SDL_CONTROLLER_AXIS_RIGHTX:
        return positive ? "Right Stick Right" : "Right Stick Left";
    case SDL_CONTROLLER_AXIS_RIGHTY:
        return positive ? "Right Stick Down" : "Right Stick Up";
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        return "LT";
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        return "RT";
    default:
        break;
    }
    return nullptr;
}

} // namespace

const char *n64_key_bind_label(N64KeyBind bind) {
    switch (bind) {
    case N64KeyBind::A:
        return "A";
    case N64KeyBind::B:
        return "B";
    case N64KeyBind::Z:
        return "Z";
    case N64KeyBind::Start:
        return "Start";
    case N64KeyBind::DPadUp:
        return "D-Pad Up";
    case N64KeyBind::DPadDown:
        return "D-Pad Down";
    case N64KeyBind::DPadLeft:
        return "D-Pad Left";
    case N64KeyBind::DPadRight:
        return "D-Pad Right";
    case N64KeyBind::L:
        return "L";
    case N64KeyBind::R:
        return "R";
    case N64KeyBind::CUp:
        return "C-Up";
    case N64KeyBind::CDown:
        return "C-Down";
    case N64KeyBind::CLeft:
        return "C-Left";
    case N64KeyBind::CRight:
        return "C-Right";
    case N64KeyBind::StickUp:
        return "Stick Up";
    case N64KeyBind::StickDown:
        return "Stick Down";
    case N64KeyBind::StickLeft:
        return "Stick Left";
    case N64KeyBind::StickRight:
        return "Stick Right";
    case N64KeyBind::Count:
        break;
    }
    return "?";
}

const char *n64_key_bind_toml_key(N64KeyBind bind) {
    switch (bind) {
    case N64KeyBind::A:
        return "a";
    case N64KeyBind::B:
        return "b";
    case N64KeyBind::Z:
        return "z";
    case N64KeyBind::Start:
        return "start";
    case N64KeyBind::DPadUp:
        return "dpad_up";
    case N64KeyBind::DPadDown:
        return "dpad_down";
    case N64KeyBind::DPadLeft:
        return "dpad_left";
    case N64KeyBind::DPadRight:
        return "dpad_right";
    case N64KeyBind::L:
        return "l";
    case N64KeyBind::R:
        return "r";
    case N64KeyBind::CUp:
        return "c_up";
    case N64KeyBind::CDown:
        return "c_down";
    case N64KeyBind::CLeft:
        return "c_left";
    case N64KeyBind::CRight:
        return "c_right";
    case N64KeyBind::StickUp:
        return "stick_up";
    case N64KeyBind::StickDown:
        return "stick_down";
    case N64KeyBind::StickLeft:
        return "stick_left";
    case N64KeyBind::StickRight:
        return "stick_right";
    case N64KeyBind::Count:
        break;
    }
    return "unknown";
}

void default_key_binds(int out[kN64KeyBindCount]) {
    out[static_cast<int>(N64KeyBind::A)] = SDL_SCANCODE_X;
    out[static_cast<int>(N64KeyBind::B)] = SDL_SCANCODE_C;
    out[static_cast<int>(N64KeyBind::Z)] = SDL_SCANCODE_Z;
    out[static_cast<int>(N64KeyBind::Start)] = SDL_SCANCODE_RETURN;
    out[static_cast<int>(N64KeyBind::DPadUp)] = SDL_SCANCODE_UP;
    out[static_cast<int>(N64KeyBind::DPadDown)] = SDL_SCANCODE_DOWN;
    out[static_cast<int>(N64KeyBind::DPadLeft)] = SDL_SCANCODE_LEFT;
    out[static_cast<int>(N64KeyBind::DPadRight)] = SDL_SCANCODE_RIGHT;
    out[static_cast<int>(N64KeyBind::L)] = SDL_SCANCODE_Q;
    out[static_cast<int>(N64KeyBind::R)] = SDL_SCANCODE_E;
    out[static_cast<int>(N64KeyBind::CUp)] = SDL_SCANCODE_I;
    out[static_cast<int>(N64KeyBind::CDown)] = SDL_SCANCODE_K;
    out[static_cast<int>(N64KeyBind::CLeft)] = SDL_SCANCODE_J;
    out[static_cast<int>(N64KeyBind::CRight)] = SDL_SCANCODE_L;
    out[static_cast<int>(N64KeyBind::StickUp)] = SDL_SCANCODE_KP_8;
    out[static_cast<int>(N64KeyBind::StickDown)] = SDL_SCANCODE_KP_2;
    out[static_cast<int>(N64KeyBind::StickLeft)] = SDL_SCANCODE_KP_4;
    out[static_cast<int>(N64KeyBind::StickRight)] = SDL_SCANCODE_KP_6;
}

void default_pad_binds(PadBind out[kN64KeyBindCount]) {
    auto button = [](SDL_GameControllerButton b) {
        return PadBind{PadBindKind::Button, static_cast<int8_t>(b)};
    };
    auto axis_pos = [](SDL_GameControllerAxis a) {
        return PadBind{PadBindKind::AxisPos, static_cast<int8_t>(a)};
    };
    auto axis_neg = [](SDL_GameControllerAxis a) {
        return PadBind{PadBindKind::AxisNeg, static_cast<int8_t>(a)};
    };

    out[static_cast<int>(N64KeyBind::A)] = button(SDL_CONTROLLER_BUTTON_A);
    out[static_cast<int>(N64KeyBind::B)] = button(SDL_CONTROLLER_BUTTON_X);
    out[static_cast<int>(N64KeyBind::Z)] =
        axis_pos(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    out[static_cast<int>(N64KeyBind::Start)] =
        button(SDL_CONTROLLER_BUTTON_START);
    out[static_cast<int>(N64KeyBind::DPadUp)] =
        button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    out[static_cast<int>(N64KeyBind::DPadDown)] =
        button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    out[static_cast<int>(N64KeyBind::DPadLeft)] =
        button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    out[static_cast<int>(N64KeyBind::DPadRight)] =
        button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    out[static_cast<int>(N64KeyBind::L)] =
        button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    out[static_cast<int>(N64KeyBind::R)] =
        button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    out[static_cast<int>(N64KeyBind::CUp)] =
        axis_neg(SDL_CONTROLLER_AXIS_RIGHTY);
    out[static_cast<int>(N64KeyBind::CDown)] =
        axis_pos(SDL_CONTROLLER_AXIS_RIGHTY);
    out[static_cast<int>(N64KeyBind::CLeft)] =
        axis_neg(SDL_CONTROLLER_AXIS_RIGHTX);
    out[static_cast<int>(N64KeyBind::CRight)] =
        axis_pos(SDL_CONTROLLER_AXIS_RIGHTX);
    out[static_cast<int>(N64KeyBind::StickUp)] =
        axis_neg(SDL_CONTROLLER_AXIS_LEFTY);
    out[static_cast<int>(N64KeyBind::StickDown)] =
        axis_pos(SDL_CONTROLLER_AXIS_LEFTY);
    out[static_cast<int>(N64KeyBind::StickLeft)] =
        axis_neg(SDL_CONTROLLER_AXIS_LEFTX);
    out[static_cast<int>(N64KeyBind::StickRight)] =
        axis_pos(SDL_CONTROLLER_AXIS_LEFTX);
}

const char *pad_bind_label(PadBind bind, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return "";
    buf[0] = '\0';
    if (bind.kind == PadBindKind::None) {
        std::snprintf(buf, buf_size, "-");
        return buf;
    }
    if (bind.kind == PadBindKind::Button) {
        const auto b = static_cast<SDL_GameControllerButton>(bind.id);
        if (const char *friendly = friendly_button_name(b)) {
            std::snprintf(buf, buf_size, "%s", friendly);
            return buf;
        }
        if (const char *s = SDL_GameControllerGetStringForButton(b)) {
            std::snprintf(buf, buf_size, "%s", s);
            return buf;
        }
        std::snprintf(buf, buf_size, "Button %d", static_cast<int>(bind.id));
        return buf;
    }
    const auto axis = static_cast<SDL_GameControllerAxis>(bind.id);
    const bool positive = bind.kind == PadBindKind::AxisPos;
    if (const char *friendly = friendly_axis_name(axis, positive)) {
        std::snprintf(buf, buf_size, "%s", friendly);
        return buf;
    }
    if (const char *s = SDL_GameControllerGetStringForAxis(axis)) {
        std::snprintf(buf, buf_size, "%c%s", positive ? '+' : '-', s);
        return buf;
    }
    std::snprintf(buf, buf_size, "Axis %d", static_cast<int>(bind.id));
    return buf;
}

bool pad_bind_to_string(PadBind bind, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return false;
    buf[0] = '\0';
    if (bind.kind == PadBindKind::None) {
        std::snprintf(buf, buf_size, "none");
        return true;
    }
    if (bind.kind == PadBindKind::Button) {
        const char *s = SDL_GameControllerGetStringForButton(
            static_cast<SDL_GameControllerButton>(bind.id));
        if (!s)
            return false;
        std::snprintf(buf, buf_size, "%s", s);
        return true;
    }
    const char *s = SDL_GameControllerGetStringForAxis(
        static_cast<SDL_GameControllerAxis>(bind.id));
    if (!s)
        return false;
    std::snprintf(buf, buf_size, "%c%s",
                  bind.kind == PadBindKind::AxisPos ? '+' : '-', s);
    return true;
}

bool pad_bind_from_string(const char *s, PadBind &out) {
    out = {};
    if (!s || !s[0] || std::strcmp(s, "none") == 0)
        return true;

    if (s[0] == '+' || s[0] == '-') {
        const bool positive = s[0] == '+';
        const SDL_GameControllerAxis axis =
            SDL_GameControllerGetAxisFromString(s + 1);
        if (axis == SDL_CONTROLLER_AXIS_INVALID)
            return false;
        out.kind = positive ? PadBindKind::AxisPos : PadBindKind::AxisNeg;
        out.id = static_cast<int8_t>(axis);
        return true;
    }

    const SDL_GameControllerButton button =
        SDL_GameControllerGetButtonFromString(s);
    if (button == SDL_CONTROLLER_BUTTON_INVALID)
        return false;
    out.kind = PadBindKind::Button;
    out.id = static_cast<int8_t>(button);
    return true;
}

void set_key_binds(const int binds[kN64KeyBindCount]) {
    std::memcpy(g_key_binds, binds, sizeof(g_key_binds));
    g_binds_ready = true;
}

void get_key_binds(int out[kN64KeyBindCount]) {
    ensure_binds();
    std::memcpy(out, g_key_binds, sizeof(g_key_binds));
}

void set_pad_binds(const PadBind binds[kN64KeyBindCount]) {
    std::memcpy(g_pad_binds, binds, sizeof(g_pad_binds));
    g_binds_ready = true;
}

void get_pad_binds(PadBind out[kN64KeyBindCount]) {
    ensure_binds();
    std::memcpy(out, g_pad_binds, sizeof(g_pad_binds));
}

void input_init() {
    SDL_GameControllerEventState(SDL_ENABLE);
    try_open_first_pad();
}

void input_shutdown() { close_pad(); }

void input_handle_event(const SDL_Event &e) {
    switch (e.type) {
    case SDL_CONTROLLERDEVICEADDED:
        if (!g_pad)
            open_pad_index(e.cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (e.cdevice.which == g_pad_instance_id) {
            close_pad();
            try_open_first_pad();
        }
        break;
    default:
        break;
    }
}

PadBind poll_pad_bind_edge() {
    try_open_first_pad();
    if (!g_pad)
        return {};

    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b) {
        const bool down = SDL_GameControllerGetButton(
                              g_pad, static_cast<SDL_GameControllerButton>(b)) !=
                          0;
        const bool was = g_prev_buttons[b];
        g_prev_buttons[b] = down;
        if (down && !was) {
            // Guide is awkward to bind; skip.
            if (b == SDL_CONTROLLER_BUTTON_GUIDE)
                continue;
            return PadBind{PadBindKind::Button, static_cast<int8_t>(b)};
        }
    }

    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; ++a) {
        const Sint16 v =
            SDL_GameControllerGetAxis(g_pad, static_cast<SDL_GameControllerAxis>(a));
        const bool pos = v >= kAxisBindThreshold;
        const bool neg = v <= -kAxisBindThreshold;
        const int pos_i = a * 2;
        const int neg_i = a * 2 + 1;
        const bool was_pos = g_prev_axes_active[pos_i];
        const bool was_neg = g_prev_axes_active[neg_i];
        g_prev_axes_active[pos_i] = pos;
        g_prev_axes_active[neg_i] = neg;
        if (pos && !was_pos)
            return PadBind{PadBindKind::AxisPos, static_cast<int8_t>(a)};
        if (neg && !was_neg)
            return PadBind{PadBindKind::AxisNeg, static_cast<int8_t>(a)};
    }
    return {};
}

Mmio::N64ControllerState sample_controller_state() {
    using namespace Mmio;
    ensure_binds();
    try_open_first_pad();

    N64ControllerState ret{};
    SDL_PumpEvents();
    const uint8_t *state = SDL_GetKeyboardState(nullptr);

    auto pressed = [&](N64KeyBind bind) {
        return key_down(state, bind) ||
               pad_bind_active(g_pad_binds[static_cast<int>(bind)]);
    };

    if (pressed(N64KeyBind::CUp))
        ret.byte2 |= N64ControllerByte2::C_UP;
    if (pressed(N64KeyBind::CDown))
        ret.byte2 |= N64ControllerByte2::C_DOWN;
    if (pressed(N64KeyBind::CLeft))
        ret.byte2 |= N64ControllerByte2::C_LEFT;
    if (pressed(N64KeyBind::CRight))
        ret.byte2 |= N64ControllerByte2::C_RIGHT;
    if (pressed(N64KeyBind::R))
        ret.byte2 |= N64ControllerByte2::R;
    if (pressed(N64KeyBind::L))
        ret.byte2 |= N64ControllerByte2::L;

    if (pressed(N64KeyBind::DPadUp))
        ret.byte1 |= N64ControllerByte1::DP_UP;
    if (pressed(N64KeyBind::DPadDown))
        ret.byte1 |= N64ControllerByte1::DP_DOWN;
    if (pressed(N64KeyBind::DPadLeft))
        ret.byte1 |= N64ControllerByte1::DP_LEFT;
    if (pressed(N64KeyBind::DPadRight))
        ret.byte1 |= N64ControllerByte1::DP_RIGHT;
    if (pressed(N64KeyBind::A))
        ret.byte1 |= N64ControllerByte1::A;
    if (pressed(N64KeyBind::B))
        ret.byte1 |= N64ControllerByte1::B;
    if (pressed(N64KeyBind::Z))
        ret.byte1 |= N64ControllerByte1::Z;
    if (pressed(N64KeyBind::Start))
        ret.byte1 |= N64ControllerByte1::START;

    if (g_pad && use_analog_left_stick()) {
        ret.joy_x =
            scale_axis_to_stick(pad_axis_value(SDL_CONTROLLER_AXIS_LEFTX));
        ret.joy_y = static_cast<int8_t>(
            -scale_axis_to_stick(pad_axis_value(SDL_CONTROLLER_AXIS_LEFTY)));
    } else {
        if (pressed(N64KeyBind::StickUp))
            ret.joy_y = 127;
        if (pressed(N64KeyBind::StickLeft))
            ret.joy_x = -127;
        if (pressed(N64KeyBind::StickDown))
            ret.joy_y = -127;
        if (pressed(N64KeyBind::StickRight))
            ret.joy_x = 127;
    }

    if (key_down(state, N64KeyBind::StickUp))
        ret.joy_y = 127;
    if (key_down(state, N64KeyBind::StickDown))
        ret.joy_y = -127;
    if (key_down(state, N64KeyBind::StickLeft))
        ret.joy_x = -127;
    if (key_down(state, N64KeyBind::StickRight))
        ret.joy_x = 127;

    return ret;
}

void poll_and_inject_controller(bool capture_keyboard) {
    Mmio::N64ControllerState ret{};
    if (!capture_keyboard)
        ret = sample_controller_state();
    Input::set_controller_state(0, ret);
}

} // namespace Ui
} // namespace N64

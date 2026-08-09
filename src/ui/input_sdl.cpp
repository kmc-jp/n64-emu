#include "ui/input_sdl.h"
#include "mmio/controller_input.h"
#include <SDL.h>

namespace N64 {
namespace Ui {

void poll_and_inject_controller(bool capture_keyboard) {
    using namespace Mmio;
    N64ControllerState ret{};
    if (!capture_keyboard) {
        SDL_PumpEvents();
        const uint8_t *state = SDL_GetKeyboardState(nullptr);
        if (state[SDL_SCANCODE_PAGEUP] | state[SDL_SCANCODE_UP])
            ret.byte2 |= N64ControllerByte2::C_UP;
        if (state[SDL_SCANCODE_PAGEDOWN] | state[SDL_SCANCODE_DOWN])
            ret.byte2 |= N64ControllerByte2::C_DOWN;
        if (state[SDL_SCANCODE_HOME] | state[SDL_SCANCODE_LEFT])
            ret.byte2 |= N64ControllerByte2::C_LEFT;
        if (state[SDL_SCANCODE_END] | state[SDL_SCANCODE_RIGHT])
            ret.byte2 |= N64ControllerByte2::C_RIGHT;
        if (state[SDL_SCANCODE_P])
            ret.byte2 |= N64ControllerByte2::R;
        if (state[SDL_SCANCODE_Q])
            ret.byte2 |= N64ControllerByte2::L;

        if (state[SDL_SCANCODE_W])
            ret.byte1 |= N64ControllerByte1::DP_UP;
        if (state[SDL_SCANCODE_S])
            ret.byte1 |= N64ControllerByte1::DP_DOWN;
        if (state[SDL_SCANCODE_A])
            ret.byte1 |= N64ControllerByte1::DP_LEFT;
        if (state[SDL_SCANCODE_D])
            ret.byte1 |= N64ControllerByte1::DP_RIGHT;
        if (state[SDL_SCANCODE_SPACE])
            ret.byte1 |= N64ControllerByte1::A;
        if (state[SDL_SCANCODE_RSHIFT])
            ret.byte1 |= N64ControllerByte1::B;
        if (state[SDL_SCANCODE_Z])
            ret.byte1 |= N64ControllerByte1::Z;
        if (state[SDL_SCANCODE_X])
            ret.byte1 |= N64ControllerByte1::START;

        if (state[SDL_SCANCODE_I])
            ret.joy_y = 127;
        if (state[SDL_SCANCODE_J])
            ret.joy_x = -127;
        if (state[SDL_SCANCODE_K])
            ret.joy_y = -127;
        if (state[SDL_SCANCODE_L])
            ret.joy_x = 127;
    }
    Input::set_controller_state(0, ret);
}

} // namespace Ui
} // namespace N64

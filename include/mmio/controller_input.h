#ifndef CONTROLLER_INPUT_H
#define CONTROLLER_INPUT_H

#include "mmio/pif.h"

namespace N64 {
namespace Input {

constexpr int kMaxControllers = 4;

void set_controller_state(int channel, const Mmio::N64ControllerState &state);
Mmio::N64ControllerState get_controller_state(int channel);

} // namespace Input
} // namespace N64

#endif

#ifndef PIF_H
#define PIF_H

#include "cpu/cpu.h"
#include "memory/bus.h"
#include "rcp/rsp.h"

namespace N64 {
namespace Mmio {

enum class JoyBusControllerType {
    // No controller connected.
    NONE,
    // Nintendo 64 Contoller. Most familiar one.
    N64_CONTROLLER,
    // Nintendo 64 Mouse
    // https://nintendo.fandom.com/wiki/Nintendo_64_Mouse
    N64_MOUSE,
};

enum class JoyBusControllerPlugin {
    TRANSFER_PAK,
    RUMBLE_PAK,
    MEM_PAK,
    RAW,
};

const JoyBusControllerType joycon_type = JoyBusControllerType::N64_CONTROLLER;
const JoyBusControllerPlugin joycon_plugin = JoyBusControllerPlugin::RUMBLE_PAK;

namespace N64ControllerByte1 {
enum N64ControllerByte1 : uint8_t {
    A = 1,
    B = 2,
    Z = 4,
    START = 8,
    DP_UP = 16,
    DP_DOWN = 32,
    DP_LEFT = 64,
    DP_RIGHT = 128,
};
}

namespace N64ControllerByte2 {
enum N64ControllerByte1 : uint8_t {
    RESET = 1,
    ZERO = 2,
    L = 4,
    R = 8,
    C_UP = 16,
    C_DOWN = 32,
    C_LEFT = 64,
    C_RIGHT = 128,
};
}

// https://en-americas-support.nintendo.com/app/answers/detail/a_id/56673/~/nintendo-64-controller-diagram
struct N64ControllerState {
    uint8_t byte1{};
    uint8_t byte2{};
    int8_t joy_x{};
    int8_t joy_y{};
};

class Pif {
  public:
    /*
      uint8_t read_addr8(uint32_t ram_addr) {
          ram_addr &= 0x7ff;
          if (ram_addr < 0x7c0) {
              Utils::unimplemented("Read from PIF Boot ROM");
              return 0;
          } else
              return ram[ram_addr & PIF_RAM_SIZE_MASK];
      }

      void write_addr8(uint32_t ram_addr, uint8_t value) {
          ram_addr &= 0x7ff;
          if (ram_addr < 0x7c0)
              Utils::critical("Write to PIF Boot ROM, ignored");
          else
              ram[ram_addr & PIF_RAM_SIZE_MASK] = value;
      }
      */

    void execute_rom_hle();

    void control_write();

    std::array<uint8_t, PIF_RAM_SIZE> ram;

  private:
    void process_controller_command(int channel, uint8_t *cmd);

    N64ControllerState poll_n64_controller() const;
};

} // namespace Mmio
} // namespace N64

#endif
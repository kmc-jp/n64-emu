#ifndef PIF_H
#define PIF_H

#include "cpu/cpu.h"
#include "memory/bus.h"
#include "rcp/rsp.h"

namespace N64 {
namespace Mmio {
namespace SI {

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

    void control_write();

    std::array<uint8_t, PIF_RAM_SIZE> ram;

  private:
    void process_controller_command(int channel, uint8_t *cmd);
};

// TODO: move this to si.h
void pif_rom_execute();

} // namespace SI
} // namespace Mmio
} // namespace N64

#endif
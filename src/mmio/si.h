#ifndef SI_H
#define SI_H

#include "memory/memory_map.h"
#include "utils/utils.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Mmio {
namespace SI {

constexpr uint32_t PADDR_SI_DRAM_ADDR = 0x04800000;
constexpr uint32_t PADDR_SI_PIF_AD_RD64B = 0x04800004;
constexpr uint32_t PADDR_SI_PIF_AD_WR64B = 0x04800010;

constexpr uint32_t PADDR_SI_STATUS = 0x04800018;

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
    void process_controller_command(int channel, uint8_t* cmd);
};

namespace SiStatusFlags {
enum SiStatusFlags : uint32_t {
    DMA_BUSY = 0x0001,
    RD_BUSY = 0x0002,
    DMA_ERROR = 0x0008,
    INTERRUPT = 0x1000,
};
}

// SI External Bus
class SI {
  public:
    SI() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    void dma_from_pif_to_dram();

    void dma_from_dram_to_pif();

    inline static SI &get_instance() { return instance; }

    Pif pif;

  private:
    uint32_t reg_dram_addr;
    uint32_t reg_pif_addr;
    uint32_t reg_status;
    bool dma_busy;

    static SI instance;
};

} // namespace SI
} // namespace Mmio

inline Mmio::SI::SI &g_si() { return Mmio::SI::SI::get_instance(); }

} // namespace N64

#endif
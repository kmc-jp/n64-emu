#ifndef RSP_H
#define RSP_H

#include "utils/pack.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Rsp {

constexpr uint32_t SP_DMEM_SIZE = 0x1000;
constexpr uint32_t SP_IMEM_SIZE = 0x1000;

constexpr uint32_t PADDR_SP_STATUS = 0x04040010;
constexpr uint32_t PADDR_SP_PC = 0x04080000;

union sp_status_write_t {
    uint32_t raw;
    PACK(struct {
        unsigned clear_halt : 1;
        unsigned set_halt : 1;
        unsigned clear_broke : 1;
        unsigned clear_intr : 1;
        unsigned set_intr : 1;
        unsigned clear_sstep : 1;
        unsigned set_sstep : 1;
        unsigned clear_intr_on_break : 1;
        unsigned set_intr_on_break : 1;
        unsigned clear_signal_0 : 1;
        unsigned set_signal_0 : 1;
        unsigned clear_signal_1 : 1;
        unsigned set_signal_1 : 1;
        unsigned clear_signal_2 : 1;
        unsigned set_signal_2 : 1;
        unsigned clear_signal_3 : 1;
        unsigned set_signal_3 : 1;
        unsigned clear_signal_4 : 1;
        unsigned set_signal_4 : 1;
        unsigned clear_signal_5 : 1;
        unsigned set_signal_5 : 1;
        unsigned clear_signal_6 : 1;
        unsigned set_signal_6 : 1;
        unsigned clear_signal_7 : 1;
        unsigned set_signal_7 : 1;
        unsigned : 7;
    });
};

static_assert(sizeof(sp_status_write_t) == 4,
              "sp_status_write_t size is not 4 bytes");

union sp_status_t {
    uint32_t raw;
    // NOTE: reverse order when using big endian machine!
    PACK(struct {
        unsigned halt : 1;
        unsigned broke : 1;
        unsigned dma_busy : 1;
        unsigned dma_full : 1;
        unsigned io_full : 1;
        unsigned single_step : 1;
        unsigned intr_on_break : 1;
        unsigned signal_0 : 1;
        unsigned signal_1 : 1;
        unsigned signal_2 : 1;
        unsigned signal_3 : 1;
        unsigned signal_4 : 1;
        unsigned signal_5 : 1;
        unsigned signal_6 : 1;
        unsigned signal_7 : 1;
        unsigned : 17;
    });
};

static_assert(sizeof(sp_status_t) == 4, "sp_status_t size is not 4 bytes");

class Rsp {
  private:
    std::array<uint8_t, SP_DMEM_SIZE> sp_dmem{};
    std::array<uint8_t, SP_IMEM_SIZE> sp_imem{};

    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/rsp_types.h#L117
    uint16_t pc, next_pc;
    sp_status_t status_reg;

    // TODO: add more registers

  public:
    Rsp() {}

    void reset();

    void step();

    void set_pc(uint16_t value);

    std::array<uint8_t, SP_DMEM_SIZE> &get_sp_dmem() { return sp_dmem; }

    std::array<uint8_t, SP_IMEM_SIZE> &get_sp_imem() { return sp_imem; }

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    void status_reg_write(uint32_t value);

    inline static Rsp &get_instance() { return instance; }

  private:
    static Rsp instance;
};

} // namespace Rsp

Rsp::Rsp &g_rsp();

} // namespace N64

#endif

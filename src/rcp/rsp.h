#ifndef RSP_H
#define RSP_H

#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "utils/utils.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Rsp {

constexpr uint32_t SP_DMEM_SIZE = 0x1000;
constexpr uint32_t SP_IMEM_SIZE = 0x1000;

constexpr uint32_t PADDR_SP_STATUS = 0x04040010;
constexpr uint32_t PADDR_SP_PC = 0x04080000;

typedef union sp_status_write {
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
} sp_status_write_t;

static_assert(sizeof(sp_status_write_t) == 4,
              "sp_status_write_t size is not 4 bytes");

typedef union sp_status {
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
} sp_status_t;

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

    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/RSP.cpp#L11
    void reset() {
        Utils::debug("Resetting RSP");
        set_pc(0);
        status_reg.raw = 0;
        status_reg.halt = 1;
        // TODO: add more registers
    }

    void step() {
        // TODO: implement
        if (status_reg.halt)
            return;

        if (status_reg.single_step)
            Utils::unimplemented("RSP single step");

        Utils::unimplemented("RSP step");
    }

    void set_pc(uint16_t value) {
        pc = value;
        next_pc = value + 4;
    }

    std::array<uint8_t, SP_DMEM_SIZE> &get_sp_dmem() { return sp_dmem; }

    std::array<uint8_t, SP_IMEM_SIZE> &get_sp_imem() { return sp_imem; }

    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/SPRegistersHandler.cpp#L67
    uint32_t read_paddr32(uint32_t paddr) const {
        switch (paddr) {
        case PADDR_SP_PC:
            return pc & 0xffc;
        default:
            Utils::abort("Unknown read from RSP paddr: {:#010x}", paddr);
        }
    }

    void write_paddr32(uint32_t paddr, uint32_t value) {
        switch (paddr) {
        case PADDR_SP_PC: {
            pc = value & 0xffc;
        } break;
        case PADDR_SP_STATUS: {
            status_reg_write(value);
        } break;
        default:
            Utils::abort("Unknown write to RSP paddr: {:#010x}", paddr);
        }
    }

    void status_reg_write(uint32_t value) {
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/SPRegistersHandler.cpp#L147
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/rsp_interface.c#L46
        sp_status_write_t write;
        write.raw = value;
        status_reg.halt =
            write.clear_halt ? 0 : (write.set_halt ? 1 : status_reg.halt);
        if (write.clear_broke)
            status_reg.broke = false;

        if (write.clear_intr) {
            g_mi().get_reg_intr().sp = false;
            N64System::check_interrupt();
        }
        if (write.set_intr) {
            g_mi().get_reg_intr().sp = true;
            N64System::check_interrupt();
        }

        status_reg.single_step =
            write.clear_sstep ? 0
                              : (write.set_sstep ? 1 : status_reg.single_step);
        status_reg.intr_on_break =
            write.clear_intr_on_break
                ? 0
                : (write.set_intr_on_break ? 1 : status_reg.intr_on_break);

        status_reg.signal_0 =
            write.clear_signal_0
                ? 0
                : (write.set_signal_0 ? 1 : status_reg.signal_0);
        status_reg.signal_1 =
            write.clear_signal_1
                ? 0
                : (write.set_signal_1 ? 1 : status_reg.signal_1);
        status_reg.signal_2 =
            write.clear_signal_2
                ? 0
                : (write.set_signal_2 ? 1 : status_reg.signal_2);
        status_reg.signal_3 =
            write.clear_signal_3
                ? 0
                : (write.set_signal_3 ? 1 : status_reg.signal_3);
        status_reg.signal_4 =
            write.clear_signal_4
                ? 0
                : (write.set_signal_4 ? 1 : status_reg.signal_4);
        status_reg.signal_5 =
            write.clear_signal_5
                ? 0
                : (write.set_signal_5 ? 1 : status_reg.signal_5);
        status_reg.signal_6 =
            write.clear_signal_6
                ? 0
                : (write.set_signal_6 ? 1 : status_reg.signal_6);
        status_reg.signal_7 =
            write.clear_signal_7
                ? 0
                : (write.set_signal_7 ? 1 : status_reg.signal_7);
    }

    inline static Rsp &get_instance() { return instance; }

  private:
    static Rsp instance;
};

} // namespace Rsp

inline Rsp::Rsp &g_rsp() { return Rsp::Rsp::get_instance(); }

} // namespace N64

#endif
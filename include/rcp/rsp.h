#ifndef RSP_H
#define RSP_H

#include "utils/pack.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Rsp {

constexpr uint32_t SP_DMEM_SIZE = 0x1000;
constexpr uint32_t SP_IMEM_SIZE = 0x1000;

// https://n64brew.dev/wiki/Reality_Signal_Processor
constexpr uint32_t PADDR_SP_MEM_ADDR = 0x04040000;
constexpr uint32_t PADDR_SP_DRAM_ADDR = 0x04040004;
constexpr uint32_t PADDR_SP_RD_LEN = 0x04040008;
constexpr uint32_t PADDR_SP_WR_LEN = 0x0404000C;
constexpr uint32_t PADDR_SP_STATUS = 0x04040010;
constexpr uint32_t PADDR_SP_DMA_FULL = 0x04040014;
constexpr uint32_t PADDR_SP_DMA_BUSY = 0x04040018;
constexpr uint32_t PADDR_SP_SEMAPHORE = 0x0404001C;
constexpr uint32_t PADDR_SP_PC = 0x04080000;
constexpr uint32_t PADDR_SP_IBIST = 0x04080004;

constexpr uint32_t RSP_DRAM_ADDR_MASK = 0xFFFFF8;
constexpr uint32_t RSP_MEM_ADDR_MASK = 0xFF8;

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

static_assert(sizeof(sp_status_write_t) == 4);

union sp_status_t {
    uint32_t raw;
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

static_assert(sizeof(sp_status_t) == 4);

union mem_addr_t {
    uint32_t raw;
    PACK(struct {
        unsigned address : 12;
        unsigned imem : 1;
        unsigned : 19;
    });
};

union dram_addr_t {
    uint32_t raw;
    PACK(struct {
        unsigned address : 24;
        unsigned : 8;
    });
};

union sp_dma_t {
    uint32_t raw;
    PACK(struct {
        unsigned length : 12;
        unsigned count : 8;
        unsigned skip : 12;
    });
};

// 128-bit VU register: 8 lanes of 16-bit, lane 0 = MSB (big-endian layout).
// https://n64brew.dev/wiki/Reality_Signal_Processor/CPU_Core
struct VuReg {
    std::array<uint16_t, 8> e{};

    uint16_t lane(int i) const { return e[static_cast<size_t>(i)]; }
    void set_lane(int i, uint16_t v) { e[static_cast<size_t>(i)] = v; }

    uint8_t byte(int i) const {
        const int lane_i = i / 2;
        const bool high = (i % 2) == 0;
        const uint16_t v = lane(lane_i);
        return high ? static_cast<uint8_t>(v >> 8) : static_cast<uint8_t>(v);
    }

    void set_byte(int i, uint8_t b) {
        const int lane_i = i / 2;
        const bool high = (i % 2) == 0;
        uint16_t v = lane(lane_i);
        if (high) {
            v = static_cast<uint16_t>((v & 0x00FF) | (static_cast<uint16_t>(b) << 8));
        } else {
            v = static_cast<uint16_t>((v & 0xFF00) | b);
        }
        set_lane(lane_i, v);
    }
};

struct AccumLane {
    int64_t value{}; // signed 48-bit logical
};

class Rsp {
  private:
    std::array<uint8_t, SP_DMEM_SIZE> sp_dmem{};
    std::array<uint8_t, SP_IMEM_SIZE> sp_imem{};

    uint16_t pc{}, next_pc{};
    bool delay_slot_{false};
    sp_status_t status_reg{};

    mem_addr_t mem_addr{};
    dram_addr_t dram_addr{};
    mem_addr_t shadow_mem_addr{};
    dram_addr_t shadow_dram_addr{};
    sp_dma_t dma{};
    bool semaphore_held{false};

    std::array<uint32_t, 32> gpr_{};
    std::array<VuReg, 32> vpr_{};
    std::array<AccumLane, 8> acc_{};
    uint16_t vcc_{};
    uint16_t vco_{};
    uint8_t vce_{};
    int16_t divin_{};
    int16_t divout_{};
    bool divin_loaded_{false};

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

    uint32_t gpr(int i) const { return i ? gpr_[static_cast<size_t>(i)] : 0; }
    void set_gpr(int i, uint32_t v) {
        if (i)
            gpr_[static_cast<size_t>(i)] = v;
    }

    int64_t acc_get(int lane) const {
        return acc_[static_cast<size_t>(lane)].value;
    }
    void acc_set(int lane, int64_t v) {
        v &= 0xFFFFFFFFFFFFLL;
        if (v & 0x800000000000LL)
            v |= ~0xFFFFFFFFFFFFLL;
        acc_[static_cast<size_t>(lane)].value = v;
    }

    VuReg &vreg(int i) { return vpr_[static_cast<size_t>(i)]; }
    uint16_t &vcc_ref() { return vcc_; }
    uint16_t &vco_ref() { return vco_; }
    uint8_t &vce_ref() { return vce_; }
    int16_t &divin_ref() { return divin_; }
    int16_t &divout_ref() { return divout_; }
    bool &divin_loaded_ref() { return divin_loaded_; }

    uint8_t dmem_load8(uint32_t addr) const;
    uint16_t dmem_load16(uint32_t addr) const;
    uint32_t dmem_load32(uint32_t addr) const;
    void dmem_store8(uint32_t addr, uint8_t v);
    void dmem_store16(uint32_t addr, uint16_t v);
    void dmem_store32(uint32_t addr, uint32_t v);

    inline static Rsp &get_instance() { return instance; }

  private:
    void dma_read();
    void dma_write();

    uint32_t fetch_instruction() const;
    void execute(uint32_t inst);
    void execute_special(uint32_t inst);
    void execute_regimm(uint32_t inst);
    void execute_cop0(uint32_t inst);
    void execute_cop2(uint32_t inst);
    void execute_lwc2(uint32_t inst);
    void execute_swc2(uint32_t inst);

    void branch(uint16_t target);
    void take_break();

    uint32_t read_cp0(int reg);
    void write_cp0(int reg, uint32_t value);

    static Rsp instance;
};

void vu_execute_compute(Rsp &rsp, uint32_t inst);
void vu_load(Rsp &rsp, uint32_t inst);
void vu_store(Rsp &rsp, uint32_t inst);


} // namespace Rsp

Rsp::Rsp &g_rsp();

} // namespace N64

#endif

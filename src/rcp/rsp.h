#ifndef RSP_H
#define RSP_H

#include "utils/utils.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Rsp {

constexpr uint32_t SP_DMEM_SIZE = 0x1000;
constexpr uint32_t SP_IMEM_SIZE = 0x1000;

constexpr uint32_t PADDR_SP_PC = 0x04080000;

class Rsp {
  private:
    std::array<uint8_t, SP_DMEM_SIZE> sp_dmem{};
    std::array<uint8_t, SP_IMEM_SIZE> sp_imem{};

    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/rsp_types.h#L117
    uint16_t pc;

  public:
    // TODO: レジスタを追加

    Rsp() {}

    void reset() {
        Utils::debug("Resetting RSP");
        // TODO: what should be done?
    }

    void step() {
        // TODO: implement
    }

    inline static Rsp &get_instance() { return instance; }

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
        case PADDR_SP_PC:
            pc = value & 0xffc;
            break;
        default:
            Utils::abort("Unknown write to RSP paddr: {:#010x}", paddr);
        }
    }

  private:
    static Rsp instance;
};

} // namespace Rsp

inline Rsp::Rsp &g_rsp() { return Rsp::Rsp::get_instance(); }

} // namespace N64

#endif
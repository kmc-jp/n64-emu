#ifndef RSP_H
#define RSP_H

#include "utils/utils.h"

namespace N64 {
namespace Rsp {

#define SP_DMEM_SIZE 0x1000
#define SP_IMEM_SIZE 0x1000

class Rsp {
  public:
    // TODO: レジスタを追加

    std::array<uint8_t, SP_DMEM_SIZE> sp_dmem{};
    std::array<uint8_t, SP_IMEM_SIZE> sp_imem{};

    Rsp() {}

    void reset() {
        Utils::info("resetting RSP");
        // TODO: what should be done?
    }

    void step() {
        // TODO: implement
    }

    inline static Rsp &get_instance() { return instance; }

  private:
    static Rsp instance;
};

} // namespace Rsp

inline Rsp::Rsp &g_rsp() { return Rsp::Rsp::get_instance(); }

} // namespace N64

#endif
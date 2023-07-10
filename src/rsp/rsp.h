#ifndef RSP_H
#define RSP_H

#include <cstdint>
#include <spdlog/spdlog.h>

namespace N64 {

#define SP_DMEM_SIZE 0x1000
#define SP_IMEM_SIZE 0x1000

class Rsp {
  public:
    // TODO: レジスタを追加

    uint8_t sp_dmem[SP_DMEM_SIZE];
    uint8_t sp_imem[SP_IMEM_SIZE];

    Rsp() {}

    void reset() {
        spdlog::debug("initializing RSP");
        // TODO: what should be done?
    }

    void step() {
        // TODO: implement
    }
};

extern Rsp n64rsp;

} // namespace N64

#endif

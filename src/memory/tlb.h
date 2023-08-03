#ifndef TLB_H
#define TLB_H

#include "utils/utils.h"
#include <cstdint>

namespace N64 {
namespace Memory {

class TLB {
  public:
    TLB() {}

    void reset() {
        Utils::debug("Resetting TLB");
        // TODO: reset TLB
    }

    inline static TLB &get_instance() { return instance; }

    void generate_exception(uint32_t vaddr) {
        Utils::critical("TLB exception: vaddr = {:#010x}", vaddr);
        Utils::unimplemented("Aborted");
    }

  private:
    static TLB instance;
};

} // namespace Memory

inline Memory::TLB &g_tlb() { return Memory::TLB::get_instance(); }

} // namespace N64

#endif

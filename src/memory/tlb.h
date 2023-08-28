#ifndef TLB_H
#define TLB_H

#include "cpu/cpu.h"
#include "utils/utils.h"
#include <cstdint>

namespace N64 {
namespace Memory {

class TLBEntry {
  public:
    // Valid bit, representing whether the entry is defined
    bool valid = false;
    entry_lo_t entry_lo0, entry_lo1;
};

class TLB {
  public:
    TLB() {}

    void reset() {
        Utils::debug("Resetting TLB");
        // TODO: reset TLB
    }

    inline static TLB &get_instance() { return instance; }

    /*
      void generate_exception(uint32_t vaddr) {
          Utils::critical("TLB exception: vaddr = {:#010x}", vaddr);
          Utils::unimplemented("Aborted");
      }
    */

    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/TLB.cpp#L113
    void write_entry(bool random) {
        int32_t index = g_cpu().cop0.reg.index & 0x3f;

        Utils::abort("todo");
    }

  private:
    static TLB instance;
};

} // namespace Memory

inline Memory::TLB &g_tlb() { return Memory::TLB::get_instance(); }

} // namespace N64

#endif

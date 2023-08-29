#ifndef TLB_H
#define TLB_H

#include "cpu/cpu.h"
#include "utils/utils.h"
#include <cstdint>

namespace N64 {

typedef PACK(union {
    uint32_t raw;
    PACK(struct {
        unsigned asid : 8;
        unsigned : 4;
        unsigned g : 1;
        unsigned vpn2 : 19;
    });
}) entry_hi_t;

static_assert(sizeof(entry_hi_t) == 4);

namespace Memory {

// TLB entry. Only 32bit mode is supported.
// See p.143
// http://datasheets.chipdb.org/NEC/Vr-Series/Vr43xx/U10504EJ7V0UMJ1.pdf
class TLBEntry {
  public:
    // Create and reset entry
    TLBEntry() : is_valid(false) {}

    bool valid() const { return is_valid; }

    void invalidate() { is_valid = false; }

    void validate(entry_lo0_t lo0, entry_lo1_t lo1, uint64_t hi,
                  uint32_t page_mask_) {
        is_valid = true;
        entry_lo0 = lo0;
        entry_lo1 = lo1;
        entry_hi.raw = hi;
        page_mask = page_mask_;
    }

  private:
    // Valid bit, representing whether the entry is defined
    bool is_valid;
    entry_lo0_t entry_lo0;
    entry_lo1_t entry_lo1;
    entry_hi_t entry_hi;
    uint32_t page_mask;
};

class TLB {
  public:
    TLB() {
        for (int i = 0; i < 32; i++) {
            entries[i] = TLBEntry();
        }
    }

    void reset() {
        Utils::debug("Resetting TLB");
        for (int i = 0; i < 32; i++) {
            entries[i] = TLBEntry();
        }
    }

    inline static TLB &get_instance() { return instance; }

    /*
      void generate_exception(uint32_t vaddr) {
          Utils::critical("TLB exception: vaddr = {:#010x}", vaddr);
          Utils::unimplemented("Aborted");
      }
    */

    void write_entry(bool random) {
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/TLB.cpp#L113
        int32_t index = g_cpu().cop0.reg.index & 0x1f;

        Utils::debug("Write to TLB entry[{}]", index);

        entries[index].validate(
            g_cpu().cop0.reg.entry_lo0, g_cpu().cop0.reg.entry_lo1,
            g_cpu().cop0.reg.entry_hi, g_cpu().cop0.reg.page_mask);
    }

  private:
    TLBEntry entries[32];

    static TLB instance;
};

} // namespace Memory

inline Memory::TLB &g_tlb() { return Memory::TLB::get_instance(); }

} // namespace N64

#endif

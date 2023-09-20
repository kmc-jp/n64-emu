#ifndef TLB_H
#define TLB_H

#include "cpu/cop0.h"
#include "cpu/cpu.h"
#include <cstdint>
#include <optional>

namespace N64 {
namespace Mmu {

enum class TLBError {
    MISS,
    INVALID,
    MODIFICATION,
    DISALLOWED_ADDRESS,
};

enum class BusAccess {
    LOAD,
    STORE,
};

// TLB entry. Only 32bit mode is supported.
// See p.143
// http://datasheets.chipdb.org/NEC/Vr-Series/Vr43xx/U10504EJ7V0UMJ1.pdf
class TLBEntry {
    friend class TLB;

  public:
    // Create and reset entry
    TLBEntry() : is_valid(false) {}

    bool valid() const { return is_valid; }

    void invalidate() { is_valid = false; }

    void validate(entry_lo0_t lo0, entry_lo1_t lo1, entry_hi_t hi,
                  uint32_t page_mask_);

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
    TLB();

    void reset();

    Cpu::ExceptionCode get_tlb_exception_code(BusAccess bus_access);

    void write_entry(bool random);

    std::optional<int> lookup_tlb_entry_index(uint32_t vaddr);

    std::optional<uint32_t> probe(uint32_t vaddr);

    TLBError get_last_error() const { return error; }

    inline static TLB &get_instance() { return instance; }

  private:
    TLBEntry entries[32];
    // Last TLB error
    TLBError error;

    static TLB instance;

    static uint32_t calculate_vpn(uint32_t vaddr, uint64_t page_mask);
};

} // namespace Mmu

Mmu::TLB &g_tlb();

} // namespace N64

#endif

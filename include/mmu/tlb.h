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
    TLBEntry() : is_valid(false), global(false) {}

    bool valid() const { return is_valid; }

    void invalidate() { is_valid = false; }

  private:
    // Valid bit, representing whether the entry is defined
    bool is_valid;
    bool global;
    entry_lo0_t entry_lo0{};
    entry_lo1_t entry_lo1{};
    entry_hi_t entry_hi{};
    uint32_t page_mask{};
};

class TLB {
  public:
    TLB();

    void reset();

    Cpu::ExceptionCode get_tlb_exception_code(BusAccess bus_access);

    void write_entry(bool random);

    void read_entry();

    void probe_index();

    std::optional<int> lookup_tlb_entry_index(uint32_t vaddr);

    std::optional<uint32_t> probe(uint32_t vaddr, BusAccess bus_access);

    TLBError get_last_error() const { return error; }

    // Update BadVAddr / Context / EntryHi after a TLB exception
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.c#L754
    static void on_tlb_exception(uint32_t vaddr);

    inline static TLB &get_instance() { return instance; }

  private:
    TLBEntry entries[32];
    // Last TLB error
    TLBError error;

    static TLB instance;

    static uint64_t calculate_vpn(uint32_t vaddr, uint32_t page_mask);
};

} // namespace Mmu

Mmu::TLB &g_tlb();

} // namespace N64

#endif

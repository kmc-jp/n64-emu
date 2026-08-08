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

// TLB entry. See R4300 manual / n64brew.
class TLBEntry {
    friend class TLB;

  public:
    TLBEntry() : is_valid(false), global(false) {}

    bool valid() const { return is_valid; }

    void invalidate() { is_valid = false; }

    bool is_global() const { return global; }
    entry_lo0_t lo0() const { return entry_lo0; }
    entry_lo1_t lo1() const { return entry_lo1; }
    entry_hi_t hi() const { return entry_hi; }
    uint32_t mask() const { return page_mask; }

  private:
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

    std::optional<int> lookup_tlb_entry_index(uint64_t vaddr);

    std::optional<uint32_t> probe(uint32_t vaddr, BusAccess bus_access);

    TLBError get_last_error() const { return error; }

    // Update BadVAddr / Context / XContext / EntryHi after a TLB exception.
    // vaddr should be the full 64-bit faulting address (sign-extend 32-bit VAs).
    static void on_tlb_exception(uint64_t vaddr);

    // Advance COP0 Random within Wired..31 (or 0..63 if Wired > 31).
    static void advance_random();

    void dump_entries() const;

    const TLBEntry &entry_at(int index) const { return entries[index & 0x1f]; }

    inline static TLB &get_instance() { return instance; }

  private:
    TLBEntry entries[32];
    TLBError error;

    static TLB instance;

    static uint64_t calculate_vpn(uint64_t vaddr, uint32_t page_mask);
    static uint64_t sign_extend_vaddr32(uint32_t vaddr);
};

} // namespace Mmu

Mmu::TLB &g_tlb();

} // namespace N64

#endif

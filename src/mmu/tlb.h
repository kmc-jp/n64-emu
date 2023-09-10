#ifndef TLB_H
#define TLB_H

#include "cpu/cpu.h"
#include "utils/utils.h"
#include <cstdint>

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

// https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/TLB.h#L25
// {
typedef union {
    uint32_t raw;
    struct {
        unsigned zero : 13;
        unsigned mask : 12;
        unsigned zero2 : 7;
    };
} page_mask_t;

static_assert(sizeof(page_mask_t) == 4);

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
                  uint32_t page_mask_) {
        is_valid = true;
        entry_lo0 = lo0;
        entry_lo1 = lo1;
        entry_hi = hi;
        page_mask.raw = page_mask_;
    }

  private:
    // Valid bit, representing whether the entry is defined
    bool is_valid;
    entry_lo0_t entry_lo0;
    entry_lo1_t entry_lo1;
    entry_hi_t entry_hi;
    page_mask_t page_mask;
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

    Cpu::ExceptionCode get_tlb_exception_code(BusAccess bus_access) {
        switch (error) {
        case TLBError::MISS: // fallthrough
        case TLBError::INVALID:
            return bus_access == BusAccess::LOAD
                       ? Cpu::ExceptionCode::TLB_MISS_LOAD
                       : Cpu::ExceptionCode::TLB_MISS_STORE;
        case TLBError::MODIFICATION:
            return Cpu::ExceptionCode::TLB_MODIFICATION;
        case TLBError::DISALLOWED_ADDRESS:
            return bus_access == BusAccess::LOAD
                       ? Cpu::ExceptionCode::ADDRESS_ERROR_LOAD
                       : Cpu::ExceptionCode::ADDRESS_ERROR_STORE;
        }
        Utils::abort("unreachable");
    }

    void write_entry(bool random) {
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/TLB.cpp#L113
        int32_t index = g_cpu().cop0.reg.index & 0x3f;

        Utils::debug("Write to TLB entry[{}]", index);
        if (index > 31) {
            Utils::abort("write to TLB entry[{}]", index);
        }

        entries[index].validate(
            g_cpu().cop0.reg.entry_lo0, g_cpu().cop0.reg.entry_lo1,
            g_cpu().cop0.reg.entry_hi, g_cpu().cop0.reg.page_mask);
    }

    // Returns index of the TLB entry which hits with the given virtual address
    std::optional<int> lookup_tlb_entry_index(uint32_t vaddr) {
        // R4300's TLB is full-assosiative.
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/TLB.cpp#L75
        for (int i = 0; i < 32; i++) {
            const TLBEntry &entry = entries[i];
            if (!entry.valid())
                continue;

            uint32_t tlb_entry_hi_raw = entries[i].entry_hi.raw;
            uint32_t mask = entries[i].page_mask.mask << 13;
            uint32_t tlb_entry_hi_raw_masked = tlb_entry_hi_raw & mask;
            uint32_t current_entry_hi_masked =
                g_cpu().cop0.reg.entry_hi.raw & mask;

            if (tlb_entry_hi_raw_masked == current_entry_hi_masked) {
                if ((tlb_entry_hi_raw & 0x100) != 0 || // Global
                    ((tlb_entry_hi_raw & 0xFF) ==
                     (g_cpu().cop0.reg.entry_hi.raw & 0xFF))) {
                    return {i};
                }
            }
        }
        return std::nullopt;
    }

    // Probe the TLB entry which hits the given address.
    // Returns the resolved address when TLB hits.
    // Otherwise, returns nullopt and set `g_tlb().error`
    std::optional<uint32_t> probe(uint32_t vaddr) {
        //g_cpu().cop0.reg.index |= 0x80000000;
        std::optional<int> tlb_entry_index = lookup_tlb_entry_index(vaddr);

        if (!tlb_entry_index.has_value()) {
            error = TLBError::MISS;
            return std::nullopt;
        }

        const TLBEntry &entry = entries[tlb_entry_index.value()];

        // TODO: add entry
        Utils::unimplemented("TLB found");

        return std::nullopt;
    }

    TLBError get_last_error() const { return error; }

    inline static TLB &get_instance() { return instance; }

  private:
    TLBEntry entries[32];
    // Last TLB error
    TLBError error;

    static TLB instance;

    static uint32_t calculate_vpn(uint32_t vaddr, uint64_t page_mask) {
        // TODO: correct?
        uint64_t mask = page_mask | 0x1fff;
        uint64_t vpn = (vaddr & 0xFFFFFFFFFF) | ((vaddr >> 22) & 0x30000000000);

        vpn &= ~mask;
        return vpn;
    }
};

} // namespace Mmu

inline Mmu::TLB &g_tlb() { return Mmu::TLB::get_instance(); }

} // namespace N64

#endif

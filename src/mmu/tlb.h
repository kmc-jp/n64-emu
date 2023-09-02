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
    // DISALLOWED_ADDRESS,
    NONE,
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
                  uint32_t page_mask_) {
        is_valid = true;
        entry_lo0 = lo0;
        entry_lo1 = lo1;
        entry_hi = hi;
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

    static inline Cpu::ExceptionCode
    get_tlb_exception_code(Mmu::BusAccess bus_access) {
        Utils::unimplemented("get_tlb_exception_code");
        exit(-1);
        return Cpu::ExceptionCode::INTERRUPT;
    }

    void write_entry(bool random) {
        // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/TLB.cpp#L113
        int32_t index = g_cpu().cop0.reg.index & 0x1f;

        Utils::debug("Write to TLB entry[{}]", index);

        entries[index].validate(
            g_cpu().cop0.reg.entry_lo0, g_cpu().cop0.reg.entry_lo1,
            g_cpu().cop0.reg.entry_hi, g_cpu().cop0.reg.page_mask);
    }

    std::optional<int> lookup_tlb_entry_index(uint32_t vaddr) {
        // R4300's TLB is full-assosiative.
        for (int i = 0; i < 32; i++) {
            const TLBEntry &entry = entries[i];
            if (!entry.valid())
                continue;

            uint64_t vaddr_vpn = calculate_vpn(vaddr, entry.page_mask);
            uint64_t entry_vpn =
                calculate_vpn(entry.entry_hi.raw, entry.page_mask);

            // Compare VPN and ASID
            if (vaddr_vpn == entry_vpn &&
                g_cpu().cop0.reg.entry_hi.asid == entry.entry_hi.asid) {
                return {i};
            }
        }
        return std::nullopt;
    }

    std::optional<uint32_t> probe(uint32_t vaddr) {
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

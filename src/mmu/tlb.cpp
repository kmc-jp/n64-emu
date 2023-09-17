#include "mmu/tlb.h"
#include "utils/utils.h"

namespace N64 {
namespace Mmu {

void TLBEntry::validate(entry_lo0_t lo0, entry_lo1_t lo1, entry_hi_t hi,
                        uint32_t page_mask_) {
    is_valid = true;
    entry_lo0 = lo0;
    entry_lo1 = lo1;
    entry_hi = hi;
    page_mask = page_mask_;
}

TLB::TLB() {
    for (int i = 0; i < 32; i++) {
        entries[i] = TLBEntry();
    }
}

void TLB::reset() {
    Utils::debug("Resetting TLB");
    for (int i = 0; i < 32; i++) {
        entries[i] = TLBEntry();
    }
}

Cpu::ExceptionCode TLB::get_tlb_exception_code(BusAccess bus_access) {
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

void TLB::write_entry(bool random) {
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/TLB.cpp#L113
    int32_t index = g_cpu().cop0.reg.index & 0x1f;

    Utils::debug("Write to TLB entry[{}]", index);

    entries[index].validate(
        g_cpu().cop0.reg.entry_lo0, g_cpu().cop0.reg.entry_lo1,
        g_cpu().cop0.reg.entry_hi, g_cpu().cop0.reg.page_mask);
}

std::optional<int> TLB::lookup_tlb_entry_index(uint32_t vaddr) {
    // R4300's TLB is full-assosiative.
    for (int i = 0; i < 32; i++) {
        const TLBEntry &entry = entries[i];
        if (!entry.valid())
            continue;

        uint64_t vaddr_vpn = calculate_vpn(vaddr, entry.page_mask);
        uint64_t entry_vpn = calculate_vpn(entry.entry_hi.raw, entry.page_mask);

        // Compare VPN and ASID
        if (vaddr_vpn == entry_vpn &&
            g_cpu().cop0.reg.entry_hi.asid == entry.entry_hi.asid) {
            return {i};
        }
    }
    return std::nullopt;
}

std::optional<uint32_t> TLB::probe(uint32_t vaddr) {
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

uint32_t TLB::calculate_vpn(uint32_t vaddr, uint64_t page_mask) {
    // TODO: correct?
    uint64_t mask = page_mask | 0x1fff;
    uint64_t vpn = (vaddr & 0xFFFFFFFFFF) | ((vaddr >> 22) & 0x30000000000);

    vpn &= ~mask;
    return vpn;
}

TLB TLB::instance{};

} // namespace Mmu

Mmu::TLB &g_tlb() { return Mmu::TLB::get_instance(); }

} // namespace N64

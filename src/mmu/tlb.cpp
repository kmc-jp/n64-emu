#include "mmu/tlb.h"
#include "utils/log.h"

namespace N64 {
namespace Mmu {

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
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/tlb_instructions.h#L27
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/TLB.cpp#L126
    auto &cop0 = g_cpu().cop0.reg;
    int32_t index = random ? (cop0.random & 0x1f) : (cop0.index & 0x1f);

    // For each pair of bits in PageMask:
    // 00/01 -> 00, 10/11 -> 11 (top bit sets both)
    uint32_t page_mask = cop0.page_mask & 0x01FFE000;
    uint32_t mask_field = (page_mask >> 13) & 0xFFF;
    uint32_t top = mask_field & 0b101010101010;
    mask_field = top | (top >> 1);
    page_mask = mask_field << 13;

    Utils::debug("TLBWI/WR entry[{}] hi={:#010x} lo0={:#010x} lo1={:#010x} "
                 "mask={:#010x} global={}",
                 index, static_cast<uint32_t>(cop0.entry_hi.raw),
                 static_cast<uint32_t>(cop0.entry_lo0.raw),
                 static_cast<uint32_t>(cop0.entry_lo1.raw), page_mask,
                 cop0.entry_lo0.global && cop0.entry_lo1.global);

    TLBEntry &entry = entries[index];
    entry.is_valid = true;
    entry.page_mask = page_mask;
    entry.entry_hi.raw = cop0.entry_hi.raw;
    entry.entry_hi.vpn2 &= ~(mask_field);
    // Clear G from EntryLo copies; use combined global
    entry.entry_lo0.raw = cop0.entry_lo0.raw & 0x03FFFFFE;
    entry.entry_lo1.raw = cop0.entry_lo1.raw & 0x03FFFFFE;
    entry.global = cop0.entry_lo0.global && cop0.entry_lo1.global;
}

void TLB::read_entry() {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/tlb_instructions.c#L29
    auto &cop0 = g_cpu().cop0.reg;
    int index = cop0.index & 0x1f;
    const TLBEntry &entry = entries[index];

    cop0.page_mask = entry.page_mask;
    cop0.entry_hi = entry.entry_hi;
    cop0.entry_lo0.raw = entry.entry_lo0.raw;
    cop0.entry_lo1.raw = entry.entry_lo1.raw;
    cop0.entry_lo0.global = entry.global;
    cop0.entry_lo1.global = entry.global;
}

void TLB::probe_index() {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/tlb_instructions.c#L15
    auto &cop0 = g_cpu().cop0.reg;
    std::optional<int> match =
        lookup_tlb_entry_index(static_cast<uint32_t>(cop0.entry_hi.raw));
    if (match.has_value()) {
        cop0.index = match.value();
    } else {
        cop0.index = 0x80000000;
    }
}

std::optional<int> TLB::lookup_tlb_entry_index(uint32_t vaddr) {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.c#L47
    // R4300's TLB is fully associative.
    for (int i = 0; i < 32; i++) {
        const TLBEntry &entry = entries[i];
        if (!entry.valid())
            continue;

        uint64_t vaddr_vpn = calculate_vpn(vaddr, entry.page_mask);
        uint64_t entry_vpn =
            calculate_vpn(entry.entry_hi.raw, entry.page_mask);

        bool vpn_match = vaddr_vpn == entry_vpn;
        bool asid_match =
            entry.global ||
            (g_cpu().cop0.reg.entry_hi.asid == entry.entry_hi.asid);

        if (vpn_match && asid_match) {
            return {i};
        }
    }
    return std::nullopt;
}

std::optional<uint32_t> TLB::probe(uint32_t vaddr, BusAccess bus_access) {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.c#L68
    std::optional<int> tlb_entry_index = lookup_tlb_entry_index(vaddr);

    if (!tlb_entry_index.has_value()) {
        error = TLBError::MISS;
        return std::nullopt;
    }

    const TLBEntry &entry = entries[tlb_entry_index.value()];

    uint32_t mask = ((entry.page_mask >> 13) << 12) | 0x0FFF;
    uint32_t page_size = mask + 1;
    bool odd = (vaddr & page_size) != 0;

    uint32_t pfn;
    bool page_valid;
    bool page_dirty;

    if (!odd) {
        page_valid = entry.entry_lo0.v;
        page_dirty = entry.entry_lo0.d;
        pfn = entry.entry_lo0.pfn;
    } else {
        page_valid = entry.entry_lo1.v;
        page_dirty = entry.entry_lo1.d;
        pfn = entry.entry_lo1.pfn;
    }

    if (!page_valid) {
        error = TLBError::INVALID;
        return std::nullopt;
    }
    if (bus_access == BusAccess::STORE && !page_dirty) {
        error = TLBError::MODIFICATION;
        return std::nullopt;
    }

    uint32_t paddr = (pfn << 12) | (vaddr & mask);
    return {paddr};
}

uint64_t TLB::calculate_vpn(uint32_t vaddr, uint32_t page_mask) {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/n64bus.c#L19
    uint64_t mask = page_mask | 0x1fff;
    uint64_t vpn = (static_cast<uint64_t>(vaddr) & 0xFFFFFFFFFFULL) |
                   ((static_cast<uint64_t>(vaddr) >> 22) & 0x30000000000ULL);
    vpn &= ~mask;
    return vpn;
}

void TLB::on_tlb_exception(uint32_t vaddr) {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.c#L754
    auto &cop0 = g_cpu().cop0.reg;
    uint32_t vpn2 = (vaddr >> 13) & 0x7FFFF;
    cop0.bad_vaddr = vaddr;
    // Context: bits [22:4] = BadVPN2, preserve PTEBase in [31:23]
    cop0.context = (cop0.context & 0xFF800000) | (vpn2 << 4);
    // EntryHi: update VPN2, keep ASID
    cop0.entry_hi.vpn2 = vpn2;
}

TLB TLB::instance{};

} // namespace Mmu

Mmu::TLB &g_tlb() { return Mmu::TLB::get_instance(); }

} // namespace N64

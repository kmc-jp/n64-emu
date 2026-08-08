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

uint64_t TLB::sign_extend_vaddr32(uint32_t vaddr) {
    return static_cast<uint64_t>(static_cast<int32_t>(vaddr));
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

void TLB::advance_random() {
    auto &cop0 = g_cpu().cop0.reg;
    const uint32_t wired = cop0.wired & 0x3F;
    const uint32_t lo = (wired > 31) ? 0 : wired;
    const uint32_t hi = (wired > 31) ? 63 : 31;
    uint32_t r = cop0.random;
    if (r < lo || r > hi) {
        r = lo;
    } else if (r >= hi) {
        r = lo;
    } else {
        r = r + 1;
    }
    cop0.random = r;
}

void TLB::dump_entries() const {
    int shown = 0;
    for (int i = 0; i < 32; i++) {
        const TLBEntry &e = entries[i];
        if (!e.valid()) {
            continue;
        }
        const uint64_t hi = e.hi().raw;
        Utils::info("TLB[{:>2}] hi={:#018x} lo0={:#010x} lo1={:#010x} "
                    "mask={:#010x} global={}",
                    i, hi, e.lo0().raw, e.lo1().raw, e.mask(), e.is_global());
        shown++;
    }
    if (shown == 0) {
        Utils::info("TLB: no valid entries");
    }
}

void TLB::write_entry(bool random) {
    auto &cop0 = g_cpu().cop0.reg;
    int32_t index;
    if (random) {
        // Ensure Random is in range, then use it.
        const uint32_t wired = cop0.wired & 0x3F;
        const uint32_t lo = (wired > 31) ? 0 : wired;
        const uint32_t hi = (wired > 31) ? 63 : 31;
        if (cop0.random < lo || cop0.random > hi) {
            cop0.random = lo;
        }
        index = static_cast<int32_t>(cop0.random & 0x1f);
        advance_random();
    } else {
        index = static_cast<int32_t>(cop0.index & 0x1f);
    }

    // For each pair of bits in PageMask:
    // 00/01 -> 00, 10/11 -> 11 (top bit sets both)
    uint32_t page_mask = cop0.page_mask & 0x01FFE000;
    uint32_t mask_field = (page_mask >> 13) & 0xFFF;
    uint32_t top = mask_field & 0b101010101010;
    mask_field = top | (top >> 1);
    page_mask = mask_field << 13;

    const uint64_t entry_hi_raw = cop0.entry_hi.raw;
    Utils::debug("TLBWI/WR entry[{}] hi={:#018x} lo0={:#010x} lo1={:#010x} "
                 "mask={:#010x} global={}",
                 index, entry_hi_raw,
                 static_cast<uint32_t>(cop0.entry_lo0.raw),
                 static_cast<uint32_t>(cop0.entry_lo1.raw), page_mask,
                 cop0.entry_lo0.global && cop0.entry_lo1.global);

    TLBEntry &entry = entries[index];
    entry.is_valid = true;
    entry.page_mask = page_mask;
    entry.entry_hi.raw = cop0.entry_hi.raw;
    entry.entry_hi.vpn2 &= ~static_cast<uint64_t>(mask_field);
    // Clear G from EntryLo copies; use combined global
    entry.entry_lo0.raw = cop0.entry_lo0.raw & 0x03FFFFFE;
    entry.entry_lo1.raw = cop0.entry_lo1.raw & 0x03FFFFFE;
    entry.global = cop0.entry_lo0.global && cop0.entry_lo1.global;
}

void TLB::read_entry() {
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
    auto &cop0 = g_cpu().cop0.reg;
    std::optional<int> match = lookup_tlb_entry_index(cop0.entry_hi.raw);
    if (match.has_value()) {
        cop0.index = match.value();
    } else {
        cop0.index = 0x80000000;
    }
}

std::optional<int> TLB::lookup_tlb_entry_index(uint64_t vaddr) {
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
    const uint64_t va64 = sign_extend_vaddr32(vaddr);
    std::optional<int> tlb_entry_index = lookup_tlb_entry_index(va64);

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

uint64_t TLB::calculate_vpn(uint64_t vaddr, uint32_t page_mask) {
    uint64_t mask = page_mask | 0x1fff;
    uint64_t vpn = (vaddr & 0xFFFFFFFFFFULL) |
                   ((vaddr >> 22) & 0x30000000000ULL);
    vpn &= ~mask;
    return vpn;
}

void TLB::on_tlb_exception(uint64_t vaddr) {
    auto &cop0 = g_cpu().cop0.reg;
    const uint64_t vpn2 = (vaddr >> 13) & 0x7FFFF;
    const uint64_t xvpn2 = (vaddr >> 13) & 0x7FFFFFF;
    const uint64_t r = (vaddr >> 62) & 0x3;

    cop0.bad_vaddr = vaddr;
    // Context: preserve PTEBase, write BadVPN2 into bits [22:4]
    cop0.context.raw =
        (cop0.context.raw & ~0x7FFFFFULL) | ((vpn2 & 0x7FFFF) << 4);
    // XContext: preserve PTEBase, write BadVPN2 and R
    cop0.xcontext.raw = (cop0.xcontext.raw & 0xFFFFFFFE00000000ULL) |
                        ((xvpn2 & 0x7FFFFFF) << 4) | (r << 31);
    // EntryHi: update VPN2 + R, keep ASID
    cop0.entry_hi.raw = (cop0.entry_hi.raw & 0xFF) | ((xvpn2 & 0x7FFFFFF) << 13) |
                        (r << 62);
}

TLB TLB::instance{};

} // namespace Mmu

Mmu::TLB &g_tlb() { return Mmu::TLB::get_instance(); }

} // namespace N64

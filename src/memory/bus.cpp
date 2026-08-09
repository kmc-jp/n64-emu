#include "memory/bus.h"
#include "cpu/cpu.h"
#include "cpu/jit/invalidate_hook.h"
#include "debugger/debugger.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "mmio/ai.h"
#include "mmio/mi.h"
#include "mmio/pi.h"
#include "mmio/si.h"
#include "mmio/vi.h"
#include "rcp/dpc.h"
#include "rcp/rsp.h"
#include "rcp/rsp_thread.h"
#include "utils/byte_array.h"
#include "utils/log.h"
#include <array>
#include <cstdint>
#include <utility>

namespace N64 {
namespace Memory {

template <class...> constexpr std::false_type always_false{};

// 64 KiB physical-page dispatch (gopher64-style). RDRAM stays a direct check.
enum class PhysMap : uint8_t {
    Unmapped = 0,
    SpMem, // 0x0400_0000: DMEM / IMEM — refine by offset
    RspReg,
    Dpc,
    Mi,
    Vi,
    Ai,
    Pi,
    Ri,
    Si,
    Sram,
    Rom,
    PifPage, // 0x1FC0_0000: PIF RAM window only
};

std::array<PhysMap, 0x10000> &phys_map() {
    static std::array<PhysMap, 0x10000> map{};
    static bool ready = false;
    if (!ready) {
        map.fill(PhysMap::Unmapped);
        const auto fill = [&](uint32_t base, uint32_t end, PhysMap kind) {
            const uint32_t lo = base >> 16;
            const uint32_t hi = end >> 16;
            for (uint32_t i = lo; i <= hi; ++i)
                map[i] = kind;
        };
        fill(PHYS_SPDMEM_BASE, PHYS_SPIMEM_END, PhysMap::SpMem);
        fill(PHYS_RSP_REG_BASE, PHYS_RSP_REG_END, PhysMap::RspReg);
        fill(PHYS_DPC_BASE, PHYS_DPC_END, PhysMap::Dpc);
        fill(PHYS_MI_BASE, PHYS_MI_END, PhysMap::Mi);
        fill(PHYS_VI_BASE, PHYS_VI_END, PhysMap::Vi);
        fill(PHYS_AI_BASE, PHYS_AI_END, PhysMap::Ai);
        fill(PHYS_PI_BASE, PHYS_PI_END, PhysMap::Pi);
        fill(PHYS_RI_BASE, PHYS_RI_END, PhysMap::Ri);
        fill(PHYS_SI_BASE, PHYS_SI_END, PhysMap::Si);
        fill(PHYS_SRAM_BASE, PHYS_SRAM_END, PhysMap::Sram);
        fill(PHYS_ROM_BASE, PHYS_ROM_END, PhysMap::Rom);
        map[PHYS_PIF_RAM_BASE >> 16] = PhysMap::PifPage;
        ready = true;
    }
    return map;
}

template <typename Wire>
[[noreturn]] void abort_unimplemented_read(uint32_t paddr) {
    constexpr bool wire64 = std::is_same<Wire, uint64_t>::value;
    constexpr bool wire32 = std::is_same<Wire, uint32_t>::value;
    constexpr bool wire16 = std::is_same<Wire, uint16_t>::value;
    constexpr bool wire8 = std::is_same<Wire, uint8_t>::value;
    static_assert(wire64 || wire32 || wire16 || wire8);

    if constexpr (wire8) {
        Utils::critical("Unimplemented read8 from paddr = {:#010x}", paddr);
    } else if constexpr (wire16) {
        Utils::critical("Unimplemented read16 from paddr = {:#010x}", paddr);
    } else if constexpr (wire32) {
        Utils::critical("Unimplemented read32 from paddr = {:#010x}", paddr);
    } else if constexpr (wire64) {
        Utils::critical("Unimplemented read64 from paddr = {:#010x}", paddr);
    } else {
        static_assert(always_false<Wire>);
    }
    Utils::abort("Aborted");
}

template <typename Wire> void abort_unimplemented_write(uint32_t paddr) {
    constexpr bool wire64 = std::is_same<Wire, uint64_t>::value;
    constexpr bool wire32 = std::is_same<Wire, uint32_t>::value;
    constexpr bool wire16 = std::is_same<Wire, uint16_t>::value;
    constexpr bool wire8 = std::is_same<Wire, uint8_t>::value;
    static_assert(wire64 || wire32 || wire16 || wire8);

    if constexpr (wire8) {
        Utils::critical("Unimplemented write8 from paddr = {:#010x}", paddr);
    } else if constexpr (wire16) {
        Utils::critical("Unimplemented write16 from paddr = {:#010x}", paddr);
    } else if constexpr (wire32) {
        Utils::critical("Unimplemented write32 from paddr = {:#010x}", paddr);
    } else if constexpr (wire64) {
        Utils::critical("Unimplemented write64 from paddr = {:#010x}", paddr);
    } else {
        static_assert(always_false<Wire>);
    }
    Utils::abort("Aborted");
}

// Do not use this function directly. Use read_paddr64, read_paddr32,
// read_paddr16 instead.
// TODO: check alignment
template <typename Wire> Wire read_paddr(uint32_t paddr) {
    constexpr bool wire64 = std::is_same<Wire, uint64_t>::value;
    constexpr bool wire32 = std::is_same<Wire, uint32_t>::value;
    constexpr bool wire16 = std::is_same<Wire, uint16_t>::value;
    constexpr bool wire8 = std::is_same<Wire, uint8_t>::value;
    static_assert(wire64 || wire32 || wire16 || wire8);

    // Hottest path: RDRAM (direct compare, no table).
    if (paddr <= PHYS_RDRAM_MEM_END) {
        return Utils::read_from_byte_array<Wire>(g_memory().get_rdram(), paddr);
    }

    switch (phys_map()[paddr >> 16]) {
    case PhysMap::SpMem:
        if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
            Rsp::g_rsp_thread().wait_idle();
            const uint32_t offs = paddr - PHYS_SPDMEM_BASE;
            auto &dmem = g_rsp().get_sp_dmem();
            if constexpr (wire8) {
                return dmem[offs & 0xFFF];
            } else if constexpr (wire16) {
                return Utils::read_from_byte_array16_be(dmem, offs & 0xFFF);
            } else if constexpr (wire32) {
                return Utils::read_from_byte_array32_be(dmem, offs & 0xFFF);
            } else if constexpr (wire64) {
                return (static_cast<uint64_t>(
                            Utils::read_from_byte_array32_be(dmem, offs & 0xFFF))
                        << 32) |
                       Utils::read_from_byte_array32_be(dmem,
                                                        (offs + 4) & 0xFFF);
            } else {
                static_assert(always_false<Wire>);
            }
        }
        if (PHYS_SPIMEM_BASE <= paddr && paddr <= PHYS_SPIMEM_END) {
            Rsp::g_rsp_thread().wait_idle();
            const uint32_t offs = paddr - PHYS_SPIMEM_BASE;
            if constexpr (wire8) {
                return Utils::read_from_byte_array8(g_rsp().get_sp_imem(), offs);
            } else if constexpr (wire16) {
                return Utils::read_from_byte_array16(g_rsp().get_sp_imem(), offs);
            } else if constexpr (wire32) {
                return Utils::read_from_byte_array32(g_rsp().get_sp_imem(), offs);
            } else if constexpr (wire64) {
                return Utils::read_from_byte_array64(g_rsp().get_sp_imem(), offs);
            } else {
                static_assert(always_false<Wire>);
            }
        }
        abort_unimplemented_read<Wire>(paddr);
    case PhysMap::RspReg:
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            return g_rsp().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    case PhysMap::Dpc:
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            return g_dpc().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    case PhysMap::Vi:
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            return g_vi().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    case PhysMap::Ai:
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            return g_ai().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    case PhysMap::Mi:
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            return g_mi().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    case PhysMap::Pi:
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            return g_pi().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    case PhysMap::Ri:
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            return g_memory().ri.read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    case PhysMap::Si:
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            return g_si().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    case PhysMap::Sram: {
        auto &sram = g_memory().get_sram();
        if (sram.empty()) {
            if constexpr (wire32) {
                return 0xFFFFFFFFu;
            } else {
                abort_unimplemented_read<Wire>(paddr);
            }
        } else if constexpr (wire32) {
            const uint32_t offs =
                (paddr - PHYS_SRAM_BASE) &
                static_cast<uint32_t>(sram.size() - 1);
            return Utils::read_from_byte_array32_be(sram, offs);
        } else {
            abort_unimplemented_read<Wire>(paddr);
        }
    }
    case PhysMap::Rom:
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            return g_memory().rom.read_offset32(paddr - PHYS_ROM_BASE);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    case PhysMap::PifPage:
        if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
            if constexpr (wire8) {
                abort_unimplemented_read<uint8_t>(paddr);
            } else if constexpr (wire16) {
                abort_unimplemented_read<uint16_t>(paddr);
            } else if constexpr (wire32) {
                uint64_t offset = paddr - PHYS_PIF_RAM_BASE;
                return Utils::read_from_byte_array32_be(g_si().pif.ram, offset);
            } else if constexpr (wire64) {
                abort_unimplemented_read<uint64_t>(paddr);
            } else {
                static_assert(always_false<Wire>);
            }
        }
        abort_unimplemented_read<Wire>(paddr);
    case PhysMap::Unmapped:
    default:
        abort_unimplemented_read<uint32_t>(paddr);
    }
}

uint64_t read_paddr64(uint32_t paddr) { return read_paddr<uint64_t>(paddr); }
uint32_t read_paddr32(uint32_t paddr) {
    if (g_debugger().has_watches()) {
        g_debugger().on_bus_access(paddr, false);
    }
    return read_paddr<uint32_t>(paddr);
}
uint16_t read_paddr16(uint32_t paddr) { return read_paddr<uint16_t>(paddr); }
uint8_t read_paddr8(uint32_t paddr) { return read_paddr<uint8_t>(paddr); }

// Do not use this function directly. Use write_paddr64, write_paddr32,
// write_paddr16 instead.
// TODO: check alignment
template <typename Wire> void write_paddr(uint32_t paddr, Wire value) {
    constexpr bool wire64 = std::is_same<Wire, uint64_t>::value;
    constexpr bool wire32 = std::is_same<Wire, uint32_t>::value;
    constexpr bool wire16 = std::is_same<Wire, uint16_t>::value;
    constexpr bool wire8 = std::is_same<Wire, uint8_t>::value;
    static_assert(wire64 || wire32 || wire16 || wire8);

    if (paddr <= PHYS_RDRAM_MEM_END) {
        if constexpr (wire8) {
            Utils::write_to_byte_array8(g_memory().get_rdram(), paddr, value);
            maybe_invalidate_code(paddr, 1);
        } else if constexpr (wire16) {
            Utils::write_to_byte_array16(g_memory().get_rdram(), paddr, value);
            maybe_invalidate_code(paddr, 2);
        } else if constexpr (wire32) {
            Utils::write_to_byte_array32(g_memory().get_rdram(), paddr, value);
            maybe_invalidate_code(paddr, 4);
        } else if constexpr (wire64) {
            Utils::write_to_byte_array64(g_memory().get_rdram(), paddr, value);
            maybe_invalidate_code(paddr, 8);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    }

    switch (phys_map()[paddr >> 16]) {
    case PhysMap::SpMem:
        if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
            Rsp::g_rsp_thread().wait_idle();
            uint32_t offs = (paddr - PHYS_SPDMEM_BASE) & 0xFFF;
            auto &dmem = g_rsp().get_sp_dmem();
            if constexpr (wire8) {
                const uint32_t word =
                    static_cast<uint32_t>(value) << (8 * (3 - (offs & 3)));
                Utils::write_to_byte_array32_be(dmem, offs & ~3u, word);
            } else if constexpr (wire16) {
                uint32_t word = static_cast<uint32_t>(value);
                if ((offs & 2) == 0)
                    word <<= 16;
                Utils::write_to_byte_array32_be(dmem, offs & ~3u, word);
            } else if constexpr (wire32) {
                Utils::write_to_byte_array32_be(dmem, offs & ~3u, value);
            } else if constexpr (wire64) {
                Utils::write_to_byte_array64_be(dmem, offs & ~7u, value);
            } else {
                static_assert(always_false<Wire>);
            }
            return;
        }
        if (PHYS_SPIMEM_BASE <= paddr && paddr <= PHYS_SPIMEM_END) {
            Rsp::g_rsp_thread().wait_idle();
            uint32_t offs = (paddr - PHYS_SPIMEM_BASE) & 0xFFF;
            auto &imem = g_rsp().get_sp_imem();
            if constexpr (wire8) {
                const uint32_t word =
                    static_cast<uint32_t>(value) << (8 * (3 - (offs & 3)));
                Utils::write_to_byte_array32(imem, offs & ~3u, word);
            } else if constexpr (wire16) {
                uint32_t word = static_cast<uint32_t>(value);
                if ((offs & 2) == 0)
                    word <<= 16;
                Utils::write_to_byte_array32(imem, offs & ~3u, word);
            } else if constexpr (wire32) {
                Utils::write_to_byte_array32(imem, offs & ~3u, value);
            } else if constexpr (wire64) {
                Utils::write_to_byte_array64(imem, offs & ~7u, value);
            } else {
                static_assert(always_false<Wire>);
            }
            return;
        }
        abort_unimplemented_write<Wire>(paddr);
        return;
    case PhysMap::RspReg:
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            g_rsp().write_paddr32(paddr, value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    case PhysMap::Dpc:
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            g_dpc().write_paddr32(paddr, value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    case PhysMap::Vi:
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            g_vi().write_paddr32(paddr, value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    case PhysMap::Ai:
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            g_ai().write_paddr32(paddr, value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    case PhysMap::Mi:
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            g_mi().write_paddr32(paddr, value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    case PhysMap::Pi:
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            g_pi().write_paddr32(paddr, value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    case PhysMap::Ri:
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            g_memory().ri.write_paddr32(paddr, value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    case PhysMap::Si:
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            g_si().write_paddr32(paddr, value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    case PhysMap::Sram: {
        auto &sram = g_memory().get_sram();
        if (sram.empty()) {
            // No cartridge SRAM — ignore writes.
        } else if constexpr (wire32) {
            const uint32_t offs =
                (paddr - PHYS_SRAM_BASE) &
                static_cast<uint32_t>(sram.size() - 1);
            Utils::write_to_byte_array32_be(sram, offs, value);
        } else {
            abort_unimplemented_write<Wire>(paddr);
        }
        return;
    }
    case PhysMap::Rom:
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            uint32_t offs = paddr - PHYS_ROM_BASE;
            Utils::write_to_byte_array32(g_memory().rom.get_raw_data(), offs,
                                         value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
        return;
    case PhysMap::PifPage:
        if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
            if constexpr (wire8) {
                abort_unimplemented_write<uint8_t>(paddr);
            } else if constexpr (wire16) {
                abort_unimplemented_write<uint16_t>(paddr);
            } else if constexpr (wire32) {
                uint64_t offs = paddr - PHYS_PIF_RAM_BASE;
                Utils::write_to_byte_array32_be(g_si().pif.ram, offs, value);
                if (paddr == 0x1FC007C0)
                    g_si().pif.control_write();
            } else if constexpr (wire64) {
                abort_unimplemented_write<uint64_t>(paddr);
            } else {
                static_assert(always_false<Wire>);
            }
            return;
        }
        abort_unimplemented_write<Wire>(paddr);
        return;
    case PhysMap::Unmapped:
    default:
        abort_unimplemented_write<uint32_t>(paddr);
        return;
    }
}

void write_paddr64(uint32_t paddr, uint64_t value) {
    write_paddr<uint64_t>(paddr, value);
}
void write_paddr32(uint32_t paddr, uint32_t value) {
    if (g_debugger().has_watches()) {
        g_debugger().on_bus_access(paddr, true);
    }
    write_paddr<uint32_t>(paddr, value);
}
void write_paddr16(uint32_t paddr, uint16_t value) {
    write_paddr<uint16_t>(paddr, value);
}
void write_paddr8(uint32_t paddr, uint8_t value) {
    write_paddr<uint8_t>(paddr, value);
}

} // namespace Memory
} // namespace N64

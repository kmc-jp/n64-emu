#include "memory/bus.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "mmio/ai.h"
#include "mmio/mi.h"
#include "mmio/pi.h"
#include "mmio/si.h"
#include "mmio/vi.h"
#include "rcp/rsp.h"
#include "utils/utils.h"
#include <cstdint>
#include <utility>

namespace N64 {
namespace Memory {

template <class...> constexpr std::false_type always_false{};

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

    if (PHYS_RDRAM_MEM_BASE <= paddr && paddr <= PHYS_RDRAM_MEM_END) {
        const uint32_t offs = paddr - PHYS_RDRAM_MEM_BASE;
        return Utils::read_from_byte_array<Wire>(g_memory().get_rdram(), offs);
    } else if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
        const uint32_t offs = paddr - PHYS_SPDMEM_BASE;
        if constexpr (wire8) {
            return Utils::read_from_byte_array8(g_rsp().get_sp_dmem(), offs);
        } else if constexpr (wire16) {
            return Utils::read_from_byte_array16(g_rsp().get_sp_dmem(), offs);
        } else if constexpr (wire32) {
            return Utils::read_from_byte_array32(g_rsp().get_sp_dmem(), offs);
        } else if constexpr (wire64) {
            return Utils::read_from_byte_array64(g_rsp().get_sp_dmem(), offs);
        } else {
            static_assert(always_false<Wire>);
        }
    } else if (PHYS_SPIMEM_BASE <= paddr && paddr <= PHYS_SPIMEM_END) {
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
    } else if (PHYS_RSP_REG_BASE <= paddr && paddr <= PHYS_RSP_REG_END) {
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
    } else if (PHYS_VI_BASE <= paddr && paddr <= PHYS_VI_END) {
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
    } else if (PHYS_AI_BASE <= paddr && paddr <= PHYS_AI_END) {
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
    } else if (PHYS_MI_BASE <= paddr && paddr <= PHYS_MI_END) {
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
    } else if (PHYS_PI_BASE <= paddr && paddr <= PHYS_PI_END) {
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
    } else if (PHYS_RI_BASE <= paddr && paddr <= PHYS_RI_END) {
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
    } else if (PHYS_SI_BASE <= paddr && paddr <= PHYS_SI_END) {
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
    } else if (PHYS_ROM_BASE <= paddr && paddr <= PHYS_ROM_END) {
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
    } else if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        if constexpr (wire8) {
            abort_unimplemented_read<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_read<uint16_t>(paddr);
        } else if constexpr (wire32) {
            uint64_t offset = paddr - PHYS_PIF_RAM_BASE;
            return Utils::read_from_byte_array32(g_si().pif.ram, offset);
        } else if constexpr (wire64) {
            abort_unimplemented_read<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    } else {
        abort_unimplemented_read<uint32_t>(paddr);
    }
}

uint64_t read_paddr64(uint32_t paddr) { return read_paddr<uint64_t>(paddr); }
uint32_t read_paddr32(uint32_t paddr) { return read_paddr<uint32_t>(paddr); }
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

    if (PHYS_RDRAM_MEM_BASE <= paddr && paddr <= PHYS_RDRAM_MEM_END) {
        uint32_t offs = paddr - PHYS_RDRAM_MEM_BASE;
        if constexpr (wire8) {
            Utils::write_to_byte_array8(g_memory().get_rdram(), offs, value);
        } else if constexpr (wire16) {
            Utils::write_to_byte_array16(g_memory().get_rdram(), offs, value);
        } else if constexpr (wire32) {
            Utils::write_to_byte_array32(g_memory().get_rdram(), offs, value);
        } else if constexpr (wire64) {
            Utils::write_to_byte_array64(g_memory().get_rdram(), offs, value);
        } else {
            static_assert(always_false<Wire>);
        }
    } else if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
        uint32_t offs = paddr - PHYS_SPDMEM_BASE;
        if constexpr (wire8) {
            Utils::write_to_byte_array8(g_rsp().get_sp_dmem(), offs, value);
        } else if constexpr (wire16) {
            Utils::write_to_byte_array16(g_rsp().get_sp_dmem(), offs, value);
        } else if constexpr (wire32) {
            Utils::write_to_byte_array32(g_rsp().get_sp_dmem(), offs, value);
        } else if constexpr (wire64) {
            Utils::write_to_byte_array64(g_rsp().get_sp_dmem(), offs, value);
        } else {
            static_assert(always_false<Wire>);
        }
    } else if (PHYS_SPIMEM_BASE <= paddr && paddr <= PHYS_SPIMEM_END) {
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            uint32_t offs = paddr - PHYS_SPIMEM_BASE;
            Utils::write_to_byte_array32(g_rsp().get_sp_imem(), offs, value);
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    } else if (PHYS_RSP_REG_BASE <= paddr && paddr <= PHYS_RSP_REG_END) {
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
    } else if (PHYS_VI_BASE <= paddr && paddr <= PHYS_VI_END) {
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
    } else if (PHYS_AI_BASE <= paddr && paddr <= PHYS_AI_END) {
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
    } else if (PHYS_MI_BASE <= paddr && paddr <= PHYS_MI_END) {
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
    } else if (PHYS_PI_BASE <= paddr && paddr <= PHYS_PI_END) {
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
    } else if (PHYS_RI_BASE <= paddr && paddr <= PHYS_RI_END) {
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
    } else if (PHYS_SI_BASE <= paddr && paddr <= PHYS_SI_END) {
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
    } else if (PHYS_ROM_BASE <= paddr && paddr <= PHYS_ROM_END) {
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
    } else if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        if constexpr (wire8) {
            abort_unimplemented_write<uint8_t>(paddr);
        } else if constexpr (wire16) {
            abort_unimplemented_write<uint16_t>(paddr);
        } else if constexpr (wire32) {
            uint64_t offs = paddr - PHYS_PIF_RAM_BASE;
            Utils::write_to_byte_array32(g_si().pif.ram, offs, value);
            // Run Joy bus commands immidiately after the last byte of pif ram
            // updated
            if (paddr == 0x1FC007C0)
                g_si().pif.control_write();
        } else if constexpr (wire64) {
            abort_unimplemented_write<uint64_t>(paddr);
        } else {
            static_assert(always_false<Wire>);
        }
    } else {
        abort_unimplemented_write<uint32_t>(paddr);
    }
}

void write_paddr64(uint32_t paddr, uint64_t value) {
    write_paddr<uint64_t>(paddr, value);
}
void write_paddr32(uint32_t paddr, uint32_t value) {
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

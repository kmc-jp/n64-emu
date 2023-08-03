#include "bus.h"
#include "memory.h"
#include "memory_map.h"
#include "mmio/ai.h"
#include "mmio/mi.h"
#include "mmio/pi.h"
#include "mmio/si.h"
#include "rcp/rsp.h"
#include "utils.h"
#include <cstdint>
#include <utility>

namespace N64 {
namespace Memory {

void abort_unimplemented_access(uint32_t paddr) {
    Utils::critical("Unimplemented access to paddr = {:#010x}", paddr);
    Utils::abort("aborted");
}

// Do not use this function directly. Use read_paddr64, read_paddr32,
// read_paddr16 instead.
template <typename Wire> Wire read_paddr(uint32_t paddr) {
    constexpr bool wire64 = std::is_same<Wire, uint64_t>::value;
    constexpr bool wire32 = std::is_same<Wire, uint32_t>::value;
    constexpr bool wire16 = std::is_same<Wire, uint16_t>::value;
    static_assert(wire64 || wire32 || wire16);

    // TODO: アラインメントのチェック
    // TODO: uint16_t, uint_64_tにちゃんと対応

    if (PHYS_RDRAM_MEM_BASE <= paddr && paddr <= PHYS_RDRAM_MEM_END) {
        const uint32_t offs = paddr - PHYS_RDRAM_MEM_BASE;
        return Utils::read_from_byte_array<Wire>(g_memory().get_rdram(), offs);
    } else if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
        const uint32_t offs = paddr - PHYS_SPDMEM_BASE;
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return Utils::read_from_byte_array32(g_rsp().get_sp_dmem(), offs);
        } else if constexpr (wire64) {
            return Utils::read_from_byte_array64(g_rsp().get_sp_dmem(), offs);
        } else {
            static_assert(false);
        }
    } else if (PHYS_SPIMEM_BASE <= paddr && paddr <= PHYS_SPIMEM_END) {
        const uint32_t offs = paddr - PHYS_SPIMEM_BASE;
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return Utils::read_from_byte_array32(g_rsp().get_sp_imem(), offs);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        } else {
            static_assert(false);
        }
    } else if (PHYS_RSP_REG_BASE <= paddr && paddr <= PHYS_RSP_REG_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_rsp().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        } else {
            static_assert(false);
        }
    } else if (PHYS_AI_BASE <= paddr && paddr <= PHYS_AI_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_ai().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        } else {
            static_assert(false);
        }
    } else if (PHYS_MI_BASE <= paddr && paddr <= PHYS_MI_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_mi().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        } else {
            static_assert(false);
        }
    } else if (PHYS_PI_BASE <= paddr && paddr <= PHYS_PI_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_pi().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        } else {
            static_assert(false);
        }
    } else if (PHYS_RI_BASE <= paddr && paddr <= PHYS_RI_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_memory().ri.read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        } else {
            static_assert(false);
        }
    } else if (PHYS_SI_BASE <= paddr && paddr <= PHYS_SI_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_si().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        } else {
            static_assert(false);
        }
    } else if (PHYS_ROM_BASE <= paddr && paddr <= PHYS_ROM_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_memory().rom.read_offset32(paddr - PHYS_ROM_BASE);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        } else {
            static_assert(false);
        }
    } else if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            uint64_t offset = paddr - PHYS_PIF_RAM_BASE;
            return Utils::read_from_byte_array32(g_si().get_pif_ram(), offset);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        } else {
            static_assert(false);
        }
    } else {
        abort_unimplemented_access(paddr);
    }
}

uint64_t read_paddr64(uint32_t paddr) { return read_paddr<uint64_t>(paddr); }
uint32_t read_paddr32(uint32_t paddr) { return read_paddr<uint32_t>(paddr); }
uint16_t read_paddr16(uint32_t paddr) { return read_paddr<uint16_t>(paddr); }

void write_paddr32(uint32_t paddr, uint32_t value) {
    // TODO: アラインメントのチェック
    if (PHYS_RDRAM_MEM_BASE <= paddr && paddr <= PHYS_RDRAM_MEM_END) {
        uint32_t offs = paddr - PHYS_RDRAM_MEM_BASE;
        Utils::write_to_byte_array32(g_memory().get_rdram(), offs, value);
    } else if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
        uint32_t offs = paddr - PHYS_SPDMEM_BASE;
        Utils::write_to_byte_array32(g_rsp().get_sp_dmem(), offs, value);
    } else if (PHYS_SPIMEM_BASE <= paddr && paddr <= PHYS_SPIMEM_END) {
        uint32_t offs = paddr - PHYS_SPIMEM_BASE;
        Utils::write_to_byte_array32(g_rsp().get_sp_imem(), offs, value);
    } else if (PHYS_RSP_REG_BASE <= paddr && paddr <= PHYS_RSP_REG_END) {
        g_rsp().write_paddr32(paddr, value);
    } else if (PHYS_AI_BASE <= paddr && paddr <= PHYS_AI_END) {
        g_ai().write_paddr32(paddr, value);
    } else if (PHYS_MI_BASE <= paddr && paddr <= PHYS_MI_END) {
        g_mi().write_paddr32(paddr, value);
    } else if (PHYS_PI_BASE <= paddr && paddr <= PHYS_PI_END) {
        g_pi().write_paddr32(paddr, value);
    } else if (PHYS_RI_BASE <= paddr && paddr <= PHYS_RI_END) {
        g_memory().ri.write_paddr32(paddr, value);
    } else if (PHYS_SI_BASE <= paddr && paddr <= PHYS_SI_END) {
        g_si().write_paddr32(paddr, value);
    } else if (PHYS_ROM_BASE <= paddr && paddr <= PHYS_ROM_END) {
        uint32_t offs = paddr - PHYS_ROM_BASE;
        Utils::write_to_byte_array32(g_memory().rom.get_raw_data(), offs,
                                     value);
    } else if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        uint64_t offs = paddr - PHYS_PIF_RAM_BASE;
        Utils::write_to_byte_array32(g_si().get_pif_ram(), offs, value);
    } else {
        Utils::critical("Unimplemented. access to paddr = {:#010x}", paddr);
        Utils::abort("aborted");
    }
}

} // namespace Memory
} // namespace N64
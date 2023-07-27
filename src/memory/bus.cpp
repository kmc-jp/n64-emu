#include "bus.h"

namespace N64 {
namespace Memory {

void abort_unimplemented_access(uint32_t paddr) {
    Utils::critical("Unimplemented access to paddr = {:#010x}", paddr);
    Utils::abort("aborted");
}

template <typename Wire> Wire read_paddr(uint32_t paddr) {
    constexpr auto wire64 = std::is_same<Wire, uint64_t>::value;
    constexpr auto wire32 = std::is_same<Wire, uint32_t>::value;
    constexpr auto wire16 = std::is_same<Wire, uint16_t>::value;
    static_assert(wire64 || wire32 || wire16);

    // TODO: アラインメントのチェック
    // TODO: uint16_t, uint_64_tにちゃんと対応

    if (PHYS_RDRAM_MEM_BASE <= paddr && paddr <= PHYS_RDRAM_MEM_END) {
        const uint32_t offs = paddr - PHYS_RDRAM_MEM_BASE;
        return Utils::read_from_byte_array<Wire>(g_memory().rdram, offs);
    } else if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            const uint32_t offs = paddr - PHYS_SPDMEM_BASE;
            return Utils::read_from_byte_array32(g_rsp().sp_dmem, offs);
        } else if constexpr (wire64) {
            const uint32_t offs = paddr - PHYS_SPDMEM_BASE;
            return Utils::read_from_byte_array64(g_rsp().sp_dmem, offs);
        }
    } else if (PHYS_SPIMEM_BASE <= paddr && paddr < PHYS_SPIMEM_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            const uint32_t offs = paddr - PHYS_SPIMEM_BASE;
            return Utils::read_from_byte_array32(g_rsp().sp_imem, offs);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        }
    } else if (PHYS_PI_BASE <= paddr && paddr <= PHYS_PI_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_pi().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        }
    } else if (PHYS_RI_BASE <= paddr && paddr <= PHYS_RI_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_memory().ri.read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        }
    } else if (PHYS_ROM_BASE <= paddr && paddr <= PHYS_ROM_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_memory().rom.read_offset32(paddr - PHYS_ROM_BASE);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        }
    } else if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        if constexpr (wire16) {
            abort_unimplemented_access(paddr);
        } else if constexpr (wire32) {
            return g_si().read_paddr32(paddr);
        } else if constexpr (wire64) {
            abort_unimplemented_access(paddr);
        }
    } else {
        abort_unimplemented_access(paddr);
    }
}

uint64_t read_paddr64(uint32_t paddr) { return read_paddr<uint64_t>(paddr); }
uint32_t read_paddr32(uint32_t paddr) { return read_paddr<uint32_t>(paddr); }
uint16_t read_paddr16(uint32_t paddr) { return read_paddr<uint16_t>(paddr); }

} // namespace Memory
} // namespace N64
#include "bus.h"

namespace N64 {
namespace Memory {

template <typename Wire> Wire read_paddr(uint32_t paddr) {
    static_assert(std::is_same<Wire, uint16_t>::value ||
                  std::is_same<Wire, uint32_t>::value);

    constexpr auto is_half = std::is_same<Wire, uint16_t>::value;

    // TODO: アラインメントのチェック
    // TODO: uint16_tにちゃんと対応

    if (PHYS_RDRAM_BASE <= paddr && paddr <= PHYS_RDRAM_END) {
        const uint32_t offs = paddr - PHYS_RDRAM_BASE;
        return Utils::read_from_byte_array<Wire>(g_memory().rdram, offs);
    } else if (PHYS_SPDMEM_BASE <= paddr && paddr <= PHYS_SPDMEM_END) {
        if constexpr (is_half) {
            Utils::critical("Unimplemented. access to paddr = {:#010x}", paddr);
            Utils::core_dump();
            exit(-1);
        } else {
            const uint32_t offs = paddr - PHYS_SPDMEM_BASE;
            return Utils::read_from_byte_array32(g_rsp().sp_dmem, offs);
        }
    } else if (PHYS_SPIMEM_BASE <= paddr && paddr < PHYS_SPIMEM_END) {
        if constexpr (is_half) {
            Utils::critical("Unsupported access to paddr = {:#010x}", paddr);
        } else {
            const uint32_t offs = paddr - PHYS_SPIMEM_BASE;
            return Utils::read_from_byte_array32(g_rsp().sp_imem, offs);
        }
    } else if (PHYS_PI_BASE <= paddr && paddr <= PHYS_PI_END) {
        if constexpr (is_half) {
            Utils::critical("Unsupported access to paddr = {:#010x}", paddr);
        } else {
            return g_pi().read_paddr32(paddr);
        }
    } else if (PHYS_RI_BASE <= paddr && paddr <= PHYS_RI_END) {
        if constexpr (is_half) {
            Utils::critical("Unsupported access to paddr = {:#010x}", paddr);
        } else {
            return g_memory().ri.read_paddr32(paddr);
        }
    } else if (PHYS_ROM_BASE <= paddr && paddr <= PHYS_ROM_END) {
        if constexpr (is_half) {
            Utils::critical("Unimplemented. access to paddr = {:#010x}", paddr);
            Utils::core_dump();
            exit(-1);
        } else {
            return g_memory().rom.read_offset32(paddr - PHYS_ROM_BASE);
        }
    } else if (PHYS_PIF_RAM_BASE <= paddr && paddr <= PHYS_PIF_RAM_END) {
        if constexpr (is_half) {
            Utils::critical("Unsupported access to paddr = {:#010x}", paddr);
        } else {
            return g_si().read_paddr32(paddr);
        }
    } else {
        Utils::critical("Unimplemented. access to paddr = {:#010x}", paddr);
        Utils::core_dump();
        exit(-1);
    }
}

uint32_t read_paddr32(uint32_t paddr) { return read_paddr<uint32_t>(paddr); }
uint16_t read_paddr16(uint32_t paddr) { return read_paddr<uint16_t>(paddr); }

} // namespace Memory
} // namespace N64
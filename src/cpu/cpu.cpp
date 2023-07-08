#include "cpu.h"
#include "../memory/bus.h"

namespace N64 {

void Cpu::step() {
    // instruction fetch
    uint32_t paddr_of_pc = Mmu::resolve_vaddr(pc);
    // spdlog::debug("vaddr 0x{:x} -> paddr 0x{:x}", pc, paddr_of_pc);

    uint32_t raw_inst = Bus::read_paddr32(paddr_of_pc);
    spdlog::debug("fetch inst = 0x{:x} from paddr = 0x{:x}", raw_inst,
                  paddr_of_pc);
    
    // TODO: decode stage

    // TODO: execute, memory, write-back stage

    spdlog::critical("Unimplemented. abort");
    N64::n64cpu.dump();
    exit(-1);
}

Cpu n64cpu{};

} // namespace N64

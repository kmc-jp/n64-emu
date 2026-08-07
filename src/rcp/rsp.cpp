#include "rcp/rsp.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "utils/log.h"

namespace N64 {
namespace Rsp {

void Rsp::reset() {
    Utils::debug("Resetting RSP");
    set_pc(0);
    status_reg.raw = 0;
    status_reg.halt = 1;
    mem_addr.raw = 0;
    dram_addr.raw = 0;
    shadow_mem_addr.raw = 0;
    shadow_dram_addr.raw = 0;
    dma.raw = 0;
    semaphore_held = false;
    sp_dmem.fill(0);
    sp_imem.fill(0);
}

void Rsp::step() {
    // RSP LLE implemented in a later pass; keep halted tasks quiet.
    if (status_reg.halt)
        return;

    if (status_reg.single_step)
        Utils::unimplemented("RSP single step");

    // Until LLE lands, ignore unhalted steps instead of aborting so OS can
    // poll SP_STATUS after DMA. Re-halt to avoid a busy spin.
    Utils::warn("RSP step stub: re-halting (LLE not ready)");
    status_reg.halt = 1;
    status_reg.broke = 1;
    if (status_reg.intr_on_break) {
        g_mi().get_reg_intr().sp = 1;
        N64System::check_interrupt();
    }
}

void Rsp::set_pc(uint16_t value) {
    pc = value & 0xffc;
    next_pc = pc + 4;
}

// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/rsp_interface.c#L74
uint32_t Rsp::read_paddr32(uint32_t paddr) const {
    switch (paddr) {
    case PADDR_SP_MEM_ADDR:
        return mem_addr.raw;
    case PADDR_SP_DRAM_ADDR:
        return dram_addr.raw;
    case PADDR_SP_RD_LEN:
    case PADDR_SP_WR_LEN:
        return dma.raw;
    case PADDR_SP_STATUS:
        return status_reg.raw;
    case PADDR_SP_DMA_FULL:
        return status_reg.dma_full;
    case PADDR_SP_DMA_BUSY:
        return status_reg.dma_busy;
    case PADDR_SP_SEMAPHORE: {
        // Non-const acquire — cast away for semaphore side effect
        auto *self = const_cast<Rsp *>(this);
        if (self->semaphore_held) {
            return 1;
        }
        self->semaphore_held = true;
        return 0;
    }
    case PADDR_SP_PC:
        return pc & 0xffc;
    case PADDR_SP_IBIST:
        return 0;
    default:
        Utils::abort("Unknown read from RSP paddr: {:#010x}", paddr);
    }
}

void Rsp::write_paddr32(uint32_t paddr, uint32_t value) {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/rsp_interface.c#L98
    switch (paddr) {
    case PADDR_SP_MEM_ADDR:
        shadow_mem_addr.raw = value;
        break;
    case PADDR_SP_DRAM_ADDR:
        shadow_dram_addr.raw = value;
        break;
    case PADDR_SP_RD_LEN:
        dma.raw = value;
        dma_read();
        break;
    case PADDR_SP_WR_LEN:
        dma.raw = value;
        dma_write();
        break;
    case PADDR_SP_STATUS:
        status_reg_write(value);
        break;
    case PADDR_SP_DMA_FULL:
    case PADDR_SP_DMA_BUSY:
        break;
    case PADDR_SP_SEMAPHORE:
        semaphore_held = false;
        break;
    case PADDR_SP_PC:
        set_pc(static_cast<uint16_t>(value));
        break;
    case PADDR_SP_IBIST:
        break;
    default:
        Utils::abort("Unknown write to RSP paddr: {:#010x}", paddr);
    }
}

void Rsp::dma_read() {
    // RDRAM -> DMEM/IMEM
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/rsp.h#L146
    uint32_t length = (dma.length + 1 + 7) & ~7u;
    uint32_t dram_address = shadow_dram_addr.address & RSP_DRAM_ADDR_MASK;
    uint32_t mem_address = shadow_mem_addr.address & RSP_MEM_ADDR_MASK;
    const bool to_imem = shadow_mem_addr.imem;

    auto &rdram = g_memory().get_rdram();
    auto &mem = to_imem ? sp_imem : sp_dmem;

    for (uint32_t i = 0; i < dma.count + 1; i++) {
        for (uint32_t j = 0; j < length; j++) {
            uint16_t addr = (mem_address + j) & 0xFFF;
            uint32_t dram_i = dram_address + j;
            mem[addr] = (dram_i < RDRAM_SIZE) ? rdram[dram_i] : 0;
        }
        uint32_t skip = (i == dma.count) ? 0 : dma.skip;
        dram_address = (dram_address + length + skip) & RSP_DRAM_ADDR_MASK;
        mem_address = (mem_address + length) & RSP_MEM_ADDR_MASK;
    }

    dram_addr.address = dram_address;
    mem_addr.address = mem_address;
    mem_addr.imem = to_imem;
    dma.raw = 0xFF8 | (dma.skip << 20);
    Utils::debug("SP DMA READ: RDRAM -> {}mem len={}", to_imem ? 'I' : 'D',
                 length);
}

void Rsp::dma_write() {
    // DMEM/IMEM -> RDRAM
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/rsp.h#L204
    uint32_t length = (dma.length + 1 + 7) & ~7u;
    uint32_t dram_address = shadow_dram_addr.address & RSP_DRAM_ADDR_MASK;
    uint32_t mem_address = shadow_mem_addr.address & RSP_MEM_ADDR_MASK;
    const bool from_imem = shadow_mem_addr.imem;

    auto &rdram = g_memory().get_rdram();
    auto &mem = from_imem ? sp_imem : sp_dmem;

    for (uint32_t i = 0; i < dma.count + 1; i++) {
        for (uint32_t j = 0; j < length; j++) {
            uint16_t addr = (mem_address + j) & 0xFFF;
            uint32_t dram_i = dram_address + j;
            if (dram_i < RDRAM_SIZE) {
                rdram[dram_i] = mem[addr];
            }
        }
        uint32_t skip = (i == dma.count) ? 0 : dma.skip;
        dram_address = (dram_address + length + skip) & RSP_DRAM_ADDR_MASK;
        mem_address = (mem_address + length) & RSP_MEM_ADDR_MASK;
    }

    dram_addr.address = dram_address;
    mem_addr.address = mem_address;
    mem_addr.imem = from_imem;
    dma.raw = 0xFF8 | (dma.skip << 20);
    Utils::debug("SP DMA WRITE: {}mem -> RDRAM len={}", from_imem ? 'I' : 'D',
                 length);
}

void Rsp::status_reg_write(uint32_t value) {
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/SPRegistersHandler.cpp#L147
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/rsp_interface.c#L46
    sp_status_write_t write;
    write.raw = value;

    if (write.clear_halt && !write.set_halt)
        status_reg.halt = 0;
    if (!write.clear_halt && write.set_halt)
        status_reg.halt = 1;

    if (write.clear_broke)
        status_reg.broke = false;

    if (write.clear_intr) {
        g_mi().get_reg_intr().sp = 0;
        N64System::check_interrupt();
    }
    if (write.set_intr) {
        g_mi().get_reg_intr().sp = 1;
        N64System::check_interrupt();
    }

    status_reg.single_step =
        write.clear_sstep ? 0 : (write.set_sstep ? 1 : status_reg.single_step);
    status_reg.intr_on_break =
        write.clear_intr_on_break
            ? 0
            : (write.set_intr_on_break ? 1 : status_reg.intr_on_break);

    status_reg.signal_0 = write.clear_signal_0
                              ? 0
                              : (write.set_signal_0 ? 1 : status_reg.signal_0);
    status_reg.signal_1 = write.clear_signal_1
                              ? 0
                              : (write.set_signal_1 ? 1 : status_reg.signal_1);
    status_reg.signal_2 = write.clear_signal_2
                              ? 0
                              : (write.set_signal_2 ? 1 : status_reg.signal_2);
    status_reg.signal_3 = write.clear_signal_3
                              ? 0
                              : (write.set_signal_3 ? 1 : status_reg.signal_3);
    status_reg.signal_4 = write.clear_signal_4
                              ? 0
                              : (write.set_signal_4 ? 1 : status_reg.signal_4);
    status_reg.signal_5 = write.clear_signal_5
                              ? 0
                              : (write.set_signal_5 ? 1 : status_reg.signal_5);
    status_reg.signal_6 = write.clear_signal_6
                              ? 0
                              : (write.set_signal_6 ? 1 : status_reg.signal_6);
    status_reg.signal_7 = write.clear_signal_7
                              ? 0
                              : (write.set_signal_7 ? 1 : status_reg.signal_7);
}

Rsp Rsp::instance{};

} // namespace Rsp

Rsp::Rsp &g_rsp() { return Rsp::Rsp::get_instance(); }

} // namespace N64

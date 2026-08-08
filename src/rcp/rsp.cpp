#include "rcp/rsp.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "debugger/debugger.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "rcp/dpc.h"
#include "utils/byte_array.h"
#include "utils/log.h"

namespace N64 {
namespace Rsp {

namespace {
constexpr uint8_t OPC_SPECIAL = 0x00;
constexpr uint8_t OPC_REGIMM = 0x01;
constexpr uint8_t OPC_J = 0x02;
constexpr uint8_t OPC_JAL = 0x03;
constexpr uint8_t OPC_BEQ = 0x04;
constexpr uint8_t OPC_BNE = 0x05;
constexpr uint8_t OPC_BLEZ = 0x06;
constexpr uint8_t OPC_BGTZ = 0x07;
constexpr uint8_t OPC_ADDI = 0x08;
constexpr uint8_t OPC_ADDIU = 0x09;
constexpr uint8_t OPC_SLTI = 0x0A;
constexpr uint8_t OPC_SLTIU = 0x0B;
constexpr uint8_t OPC_ANDI = 0x0C;
constexpr uint8_t OPC_ORI = 0x0D;
constexpr uint8_t OPC_XORI = 0x0E;
constexpr uint8_t OPC_LUI = 0x0F;
constexpr uint8_t OPC_COP0 = 0x10;
constexpr uint8_t OPC_COP2 = 0x12;
constexpr uint8_t OPC_LB = 0x20;
constexpr uint8_t OPC_LH = 0x21;
constexpr uint8_t OPC_LW = 0x23;
constexpr uint8_t OPC_LBU = 0x24;
constexpr uint8_t OPC_LHU = 0x25;
constexpr uint8_t OPC_SB = 0x28;
constexpr uint8_t OPC_SH = 0x29;
constexpr uint8_t OPC_SW = 0x2B;
constexpr uint8_t OPC_LWC2 = 0x32;
constexpr uint8_t OPC_SWC2 = 0x3A;

inline uint8_t op(uint32_t i) { return static_cast<uint8_t>((i >> 26) & 0x3F); }
inline uint8_t rs(uint32_t i) { return static_cast<uint8_t>((i >> 21) & 0x1F); }
inline uint8_t rt(uint32_t i) { return static_cast<uint8_t>((i >> 16) & 0x1F); }
inline uint8_t rd(uint32_t i) { return static_cast<uint8_t>((i >> 11) & 0x1F); }
inline uint8_t sa(uint32_t i) { return static_cast<uint8_t>((i >> 6) & 0x1F); }
inline uint8_t funct(uint32_t i) { return static_cast<uint8_t>(i & 0x3F); }
inline int16_t imm_se(uint32_t i) {
    return static_cast<int16_t>(i & 0xFFFF);
}
inline uint16_t imm_ze(uint32_t i) { return static_cast<uint16_t>(i & 0xFFFF); }
inline uint32_t target(uint32_t i) { return (i & 0x03FFFFFF) << 2; }
} // namespace

void Rsp::reset() {
    Utils::debug("Resetting RSP");
    set_pc(0);
    delay_slot_ = false;
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
    gpr_.fill(0);
    for (auto &v : vpr_)
        v.e.fill(0);
    for (auto &a : acc_)
        a.value = 0;
    vcc_ = 0;
    vco_ = 0;
    vce_ = 0;
    divin_ = 0;
    divout_ = 0;
    divin_loaded_ = false;
}

void Rsp::set_pc(uint16_t value) {
    pc = value & 0xffc;
    next_pc = (pc + 4) & 0xffc;
    delay_slot_ = false;
}

void Rsp::branch(uint16_t target_pc) {
    next_pc = target_pc & 0xffc;
}

void Rsp::take_break() {
    // https://n64brew.dev/wiki/Reality_Signal_Processor/CPU_Core
    status_reg.halt = 1;
    status_reg.broke = 1;
    const uint32_t type = dmem_load32(0xFC0);
    if (type != 2) {
        Utils::info("RSP BREAK pc={:#x} OSTask type={}", pc, type);
    }
    if (status_reg.intr_on_break) {
        g_mi().get_reg_intr().sp = 1;
        N64System::check_interrupt();
    }
}

uint8_t Rsp::dmem_load8(uint32_t addr) const {
    return sp_dmem[addr & 0xFFF];
}

uint16_t Rsp::dmem_load16(uint32_t addr) const {
    // RSP allows misaligned DMEM access (n64brew).
    const uint32_t a = addr & 0xFFF;
    return static_cast<uint16_t>((sp_dmem[a] << 8) | sp_dmem[(a + 1) & 0xFFF]);
}

uint32_t Rsp::dmem_load32(uint32_t addr) const {
    const uint32_t a = addr & 0xFFF;
    return (static_cast<uint32_t>(sp_dmem[a]) << 24) |
           (static_cast<uint32_t>(sp_dmem[(a + 1) & 0xFFF]) << 16) |
           (static_cast<uint32_t>(sp_dmem[(a + 2) & 0xFFF]) << 8) |
           static_cast<uint32_t>(sp_dmem[(a + 3) & 0xFFF]);
}

void Rsp::dmem_store8(uint32_t addr, uint8_t v) {
    sp_dmem[addr & 0xFFF] = v;
}

void Rsp::dmem_store16(uint32_t addr, uint16_t v) {
    const uint32_t a = addr & 0xFFF;
    sp_dmem[a] = static_cast<uint8_t>(v >> 8);
    sp_dmem[(a + 1) & 0xFFF] = static_cast<uint8_t>(v);
}

void Rsp::dmem_store32(uint32_t addr, uint32_t v) {
    const uint32_t a = addr & 0xFFF;
    sp_dmem[a] = static_cast<uint8_t>(v >> 24);
    sp_dmem[(a + 1) & 0xFFF] = static_cast<uint8_t>(v >> 16);
    sp_dmem[(a + 2) & 0xFFF] = static_cast<uint8_t>(v >> 8);
    sp_dmem[(a + 3) & 0xFFF] = static_cast<uint8_t>(v);
}

uint32_t Rsp::fetch_instruction() const {
    const uint32_t a = pc & 0xFFC;
    return Utils::read_from_byte_array32(sp_imem, a);
}

void Rsp::step() {
    if (status_reg.halt)
        return;

    const uint32_t inst = fetch_instruction();
    const uint16_t cur = pc;
    pc = next_pc;
    next_pc = (pc + 4) & 0xffc;
    delay_slot_ = false;

    execute(inst, cur);

    if (status_reg.single_step) {
        status_reg.halt = 1;
    }
}

void Rsp::execute(uint32_t inst, uint16_t inst_pc) {
    switch (op(inst)) {
    case OPC_SPECIAL:
        execute_special(inst);
        break;
    case OPC_REGIMM:
        execute_regimm(inst);
        break;
    case OPC_J: {
        delay_slot_ = true;
        branch(static_cast<uint16_t>(target(inst)));
    } break;
    case OPC_JAL: {
        delay_slot_ = true;
        // Link = address of instruction after the delay slot.
        set_gpr(31, (pc + 4) & 0xffc);
        branch(static_cast<uint16_t>(target(inst)));
    } break;
    case OPC_BEQ: {
        delay_slot_ = true;
        if (gpr(rs(inst)) == gpr(rt(inst)))
            branch(static_cast<uint16_t>(pc + (imm_se(inst) << 2)));
    } break;
    case OPC_BNE: {
        delay_slot_ = true;
        if (gpr(rs(inst)) != gpr(rt(inst)))
            branch(static_cast<uint16_t>(pc + (imm_se(inst) << 2)));
    } break;
    case OPC_BLEZ: {
        delay_slot_ = true;
        if (static_cast<int32_t>(gpr(rs(inst))) <= 0)
            branch(static_cast<uint16_t>(pc + (imm_se(inst) << 2)));
    } break;
    case OPC_BGTZ: {
        delay_slot_ = true;
        if (static_cast<int32_t>(gpr(rs(inst))) > 0)
            branch(static_cast<uint16_t>(pc + (imm_se(inst) << 2)));
    } break;
    case OPC_ADDI:
    case OPC_ADDIU:
        set_gpr(rt(inst), gpr(rs(inst)) + static_cast<uint32_t>(imm_se(inst)));
        break;
    case OPC_SLTI:
        set_gpr(rt(inst),
                static_cast<int32_t>(gpr(rs(inst))) < imm_se(inst) ? 1 : 0);
        break;
    case OPC_SLTIU:
        set_gpr(rt(inst), gpr(rs(inst)) < static_cast<uint32_t>(imm_se(inst))
                              ? 1
                              : 0);
        break;
    case OPC_ANDI:
        set_gpr(rt(inst), gpr(rs(inst)) & imm_ze(inst));
        break;
    case OPC_ORI:
        set_gpr(rt(inst), gpr(rs(inst)) | imm_ze(inst));
        break;
    case OPC_XORI:
        set_gpr(rt(inst), gpr(rs(inst)) ^ imm_ze(inst));
        break;
    case OPC_LUI:
        set_gpr(rt(inst), static_cast<uint32_t>(imm_ze(inst)) << 16);
        break;
    case OPC_COP0:
        execute_cop0(inst);
        break;
    case OPC_COP2:
        execute_cop2(inst);
        break;
    case OPC_LB:
        set_gpr(rt(inst), static_cast<int32_t>(static_cast<int8_t>(
                              dmem_load8(gpr(rs(inst)) + imm_se(inst)))));
        break;
    case OPC_LBU:
        set_gpr(rt(inst), dmem_load8(gpr(rs(inst)) + imm_se(inst)));
        break;
    case OPC_LH:
        set_gpr(rt(inst), static_cast<int32_t>(static_cast<int16_t>(
                              dmem_load16(gpr(rs(inst)) + imm_se(inst)))));
        break;
    case OPC_LHU:
        set_gpr(rt(inst), dmem_load16(gpr(rs(inst)) + imm_se(inst)));
        break;
    case OPC_LW:
        set_gpr(rt(inst), dmem_load32(gpr(rs(inst)) + imm_se(inst)));
        break;
    case OPC_SB:
        dmem_store8(gpr(rs(inst)) + imm_se(inst),
                    static_cast<uint8_t>(gpr(rt(inst))));
        break;
    case OPC_SH:
        dmem_store16(gpr(rs(inst)) + imm_se(inst),
                     static_cast<uint16_t>(gpr(rt(inst))));
        break;
    case OPC_SW:
        dmem_store32(gpr(rs(inst)) + imm_se(inst), gpr(rt(inst)));
        break;
    case OPC_LWC2:
        execute_lwc2(inst);
        break;
    case OPC_SWC2:
        execute_swc2(inst);
        break;
    default:
        Utils::warn("RSP unknown opcode {:#04x} inst={:#010x} pc={:#05x}",
                    op(inst), inst, inst_pc);
        break;
    }
}

void Rsp::execute_special(uint32_t inst) {
    switch (funct(inst)) {
    case 0x00: // SLL
        set_gpr(rd(inst), gpr(rt(inst)) << sa(inst));
        break;
    case 0x02: // SRL
        set_gpr(rd(inst), gpr(rt(inst)) >> sa(inst));
        break;
    case 0x03: // SRA
        set_gpr(rd(inst), static_cast<uint32_t>(static_cast<int32_t>(gpr(rt(inst))) >>
                                                 sa(inst)));
        break;
    case 0x04: // SLLV
        set_gpr(rd(inst), gpr(rt(inst)) << (gpr(rs(inst)) & 31));
        break;
    case 0x06: // SRLV
        set_gpr(rd(inst), gpr(rt(inst)) >> (gpr(rs(inst)) & 31));
        break;
    case 0x07: // SRAV
        set_gpr(rd(inst),
                static_cast<uint32_t>(static_cast<int32_t>(gpr(rt(inst))) >>
                                     (gpr(rs(inst)) & 31)));
        break;
    case 0x08: { // JR
        delay_slot_ = true;
        branch(static_cast<uint16_t>(gpr(rs(inst))));
    } break;
    case 0x09: { // JALR
        delay_slot_ = true;
        const uint32_t link = (pc + 4) & 0xffc;
        branch(static_cast<uint16_t>(gpr(rs(inst))));
        set_gpr(rd(inst), link); // rd==0 discards (r0)
    } break;
    case 0x0D: // BREAK
        take_break();
        break;
    case 0x20: // ADD
    case 0x21: // ADDU
        set_gpr(rd(inst), gpr(rs(inst)) + gpr(rt(inst)));
        break;
    case 0x22: // SUB
    case 0x23: // SUBU
        set_gpr(rd(inst), gpr(rs(inst)) - gpr(rt(inst)));
        break;
    case 0x24: // AND
        set_gpr(rd(inst), gpr(rs(inst)) & gpr(rt(inst)));
        break;
    case 0x25: // OR
        set_gpr(rd(inst), gpr(rs(inst)) | gpr(rt(inst)));
        break;
    case 0x26: // XOR
        set_gpr(rd(inst), gpr(rs(inst)) ^ gpr(rt(inst)));
        break;
    case 0x27: // NOR
        set_gpr(rd(inst), ~(gpr(rs(inst)) | gpr(rt(inst))));
        break;
    case 0x2A: // SLT
        set_gpr(rd(inst), static_cast<int32_t>(gpr(rs(inst))) <
                                  static_cast<int32_t>(gpr(rt(inst)))
                              ? 1
                              : 0);
        break;
    case 0x2B: // SLTU
        set_gpr(rd(inst), gpr(rs(inst)) < gpr(rt(inst)) ? 1 : 0);
        break;
    default:
        Utils::warn("RSP SPECIAL funct={:#04x}", funct(inst));
        break;
    }
}

void Rsp::execute_regimm(uint32_t inst) {
    const uint8_t rt_f = rt(inst);
    delay_slot_ = true;
    const int32_t rs_val = static_cast<int32_t>(gpr(rs(inst)));
    bool take = false;
    switch (rt_f) {
    case 0x00: // BLTZ
        take = rs_val < 0;
        break;
    case 0x01: // BGEZ
        take = rs_val >= 0;
        break;
    case 0x10: // BLTZAL
        take = rs_val < 0;
        set_gpr(31, (pc + 4) & 0xffc);
        break;
    case 0x11: // BGEZAL
        take = rs_val >= 0;
        set_gpr(31, (pc + 4) & 0xffc);
        break;
    default:
        Utils::warn("RSP REGIMM rt={:#04x}", rt_f);
        delay_slot_ = false;
        return;
    }
    if (take)
        branch(static_cast<uint16_t>(pc + (imm_se(inst) << 2)));
}

uint32_t Rsp::read_cp0(int reg) {
    // RSP COP0 maps SP DMA/status and DPC registers.
    // https://n64brew.dev/wiki/Reality_Signal_Processor
    switch (reg) {
    case 0:
        return mem_addr.raw;
    case 1:
        return dram_addr.raw;
    case 2:
    case 3:
        return dma.raw;
    case 4:
        return status_reg.raw;
    case 5:
        return status_reg.dma_full;
    case 6:
        return status_reg.dma_busy;
    case 7: {
        if (semaphore_held)
            return 1;
        semaphore_held = true;
        return 0;
    }
    case 8:
        return g_dpc().get_start();
    case 9:
        return g_dpc().get_end();
    case 10:
        return g_dpc().get_current();
    case 11:
        return g_dpc().get_status().raw;
    case 12:
        return 0; // clock
    case 13:
    case 14:
    case 15:
        return 0;
    default:
        return 0;
    }
}

void Rsp::write_cp0(int reg, uint32_t value) {
    switch (reg) {
    case 0:
        shadow_mem_addr.raw = value;
        break;
    case 1:
        shadow_dram_addr.raw = value;
        break;
    case 2:
        dma.raw = value;
        // nbytes-1 == 0xffffffff means a 0-byte transfer. The 12/8/12
        // bitfields would otherwise decode to a 256×4096 wipe; treat as
        // completed empty DMA and mirror hardware's post-transfer length.
        if (value == 0xffffffffu) {
            dma.raw = 0xFF8;
            break;
        }
        dma_read();
        break;
    case 3:
        dma.raw = value;
        if (value == 0xffffffffu) {
            dma.raw = 0xFF8;
            break;
        }
        dma_write();
        break;
    case 4:
        status_reg_write(value);
        break;
    case 7:
        semaphore_held = false;
        break;
    case 8:
        g_dpc().write_paddr32(Rdp::PADDR_DPC_START, value);
        break;
    case 9:
        g_dpc().write_paddr32(Rdp::PADDR_DPC_END, value);
        break;
    case 11:
        g_dpc().write_paddr32(Rdp::PADDR_DPC_STATUS, value);
        break;
    default:
        break;
    }
}

void Rsp::execute_cop0(uint32_t inst) {
    const uint8_t sub = rs(inst);
    if (sub == 0x00) { // MFC0
        set_gpr(rt(inst), read_cp0(rd(inst)));
    } else if (sub == 0x04) { // MTC0
        write_cp0(rd(inst), gpr(rt(inst)));
    } else {
        Utils::warn("RSP COP0 sub={:#04x}", sub);
    }
}

void Rsp::execute_cop2(uint32_t inst) {
    // Vector compute group when bit 25 set; else move/control.
    if (inst & (1u << 25)) {
        vu_execute_compute(*this, inst);
        return;
    }
    const uint8_t sub = rs(inst);
    const uint8_t vd = rd(inst);
    const uint8_t vt = rt(inst);
    const uint8_t element = sa(inst) >> 1; // rough; MFC2/MTC2 use element
    switch (sub) {
    case 0x00: { // MFC2
        const int elem = (inst >> 7) & 0xF;
        const uint16_t hi = vpr_[vd].byte(elem);
        const uint16_t lo = vpr_[vd].byte((elem + 1) & 15);
        int16_t val = static_cast<int16_t>((hi << 8) | lo);
        set_gpr(vt, static_cast<int32_t>(val));
        (void)element;
    } break;
    case 0x02: { // CFC2
        uint32_t val = 0;
        switch (vd & 3) {
        case 0:
            val = vco_;
            break;
        case 1:
            val = vcc_;
            break;
        case 2:
            val = vce_;
            break;
        default:
            val = 0;
            break;
        }
        set_gpr(vt, static_cast<int32_t>(static_cast<int16_t>(val)));
    } break;
    case 0x04: { // MTC2
        const int elem = (inst >> 7) & 0xF;
        const uint32_t val = gpr(vt);
        vpr_[vd].set_byte(elem, static_cast<uint8_t>(val >> 8));
        // Low byte is dropped (not wrapped) when element == 15.
        if (elem < 15)
            vpr_[vd].set_byte(elem + 1, static_cast<uint8_t>(val));
    } break;
    case 0x06: { // CTC2
        const uint16_t val = static_cast<uint16_t>(gpr(vt));
        switch (vd & 3) {
        case 0:
            vco_ = val;
            break;
        case 1:
            vcc_ = val;
            break;
        case 2:
            vce_ = static_cast<uint8_t>(val);
            break;
        default:
            break;
        }
    } break;
    default:
        Utils::warn("RSP COP2 move sub={:#04x}", sub);
        break;
    }
}

void Rsp::execute_lwc2(uint32_t inst) { vu_load(*this, inst); }
void Rsp::execute_swc2(uint32_t inst) { vu_store(*this, inst); }

// --- SP DMA / MMIO (unchanged behavior from prior commit) ---

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
        auto *self = const_cast<Rsp *>(this);
        if (self->semaphore_held)
            return 1;
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
    switch (paddr) {
    case PADDR_SP_MEM_ADDR:
        shadow_mem_addr.raw = value;
        break;
    case PADDR_SP_DRAM_ADDR:
        shadow_dram_addr.raw = value;
        break;
    case PADDR_SP_RD_LEN:
        dma.raw = value;
        if (value == 0xffffffffu) {
            dma.raw = 0xFF8;
        } else {
            dma_read();
        }
        break;
    case PADDR_SP_WR_LEN:
        dma.raw = value;
        if (value == 0xffffffffu) {
            dma.raw = 0xFF8;
        } else {
            dma_write();
        }
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
    uint32_t length = (dma.length + 1 + 7) & ~7u;
    uint32_t dram_address = shadow_dram_addr.address & RSP_DRAM_ADDR_MASK;
    uint32_t mem_address = shadow_mem_addr.address & RSP_MEM_ADDR_MASK;
    const bool to_imem = shadow_mem_addr.imem;
    auto &rdram = g_memory().get_rdram();
    auto &mem = to_imem ? sp_imem : sp_dmem;

    for (uint32_t i = 0; i < dma.count + 1; i++) {
        for (uint32_t j = 0; j < length; j++) {
            uint16_t addr = (mem_address + j) & 0xFFF;
            // IMEM matches RDRAM host-endian layout; DMEM is big-endian so
            // convert with byte^3 (Dillonb rsp_dma_read).
            uint16_t index =
                to_imem ? addr
                        : static_cast<uint16_t>(Utils::byte_address(addr));
            uint32_t dram_i = dram_address + j;
            mem[index] = (dram_i < RDRAM_SIZE) ? rdram[dram_i] : 0;
        }
        uint32_t skip = (i == dma.count) ? 0 : dma.skip;
        dram_address = (dram_address + length + skip) & RSP_DRAM_ADDR_MASK;
        mem_address = (mem_address + length) & RSP_MEM_ADDR_MASK;
    }
    dram_addr.address = dram_address;
    mem_addr.address = mem_address;
    mem_addr.imem = to_imem;
    dma.raw = 0xFF8 | (dma.skip << 20);
}

void Rsp::dma_write() {
    uint32_t length = (dma.length + 1 + 7) & ~7u;
    uint32_t dram_address = shadow_dram_addr.address & RSP_DRAM_ADDR_MASK;
    uint32_t mem_address = shadow_mem_addr.address & RSP_MEM_ADDR_MASK;
    const bool from_imem = shadow_mem_addr.imem;
    auto &rdram = g_memory().get_rdram();
    auto &mem = from_imem ? sp_imem : sp_dmem;

    for (uint32_t i = 0; i < dma.count + 1; i++) {
        for (uint32_t j = 0; j < length; j++) {
            uint16_t addr = (mem_address + j) & 0xFFF;
            uint16_t index =
                from_imem ? addr
                          : static_cast<uint16_t>(Utils::byte_address(addr));
            uint32_t dram_i = dram_address + j;
            if (dram_i < RDRAM_SIZE)
                rdram[dram_i] = mem[index];
        }
        uint32_t skip = (i == dma.count) ? 0 : dma.skip;
        dram_address = (dram_address + length + skip) & RSP_DRAM_ADDR_MASK;
        mem_address = (mem_address + length) & RSP_MEM_ADDR_MASK;
    }
    dram_addr.address = dram_address;
    mem_addr.address = mem_address;
    mem_addr.imem = from_imem;
    dma.raw = 0xFF8 | (dma.skip << 20);
}

void Rsp::status_reg_write(uint32_t value) {
    sp_status_write_t write;
    write.raw = value;

    if (write.clear_halt && !write.set_halt) {
        const bool was_halted = status_reg.halt != 0;
        status_reg.halt = 0;
        if (was_halted) {
            const uint32_t type = dmem_load32(0xFC0);
            if (type != 2) {
                Utils::info("RSP unhalt OSTask type={}", type);
            }
            g_debugger().on_rsp_unhalt();
        }
    }
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

#include "rcp/rsp.h"
#include "cpu/jit/invalidate_hook.h"
#include "debugger/debugger.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "mmio/mi.h"
#include "n64_system/interrupt.h"
#include "n64_system/scheduler.h"
#include "rcp/dpc.h"
#include "rcp/vu_profile.h"
#include "rdp/rdp_core.h"
#include "utils/byte_array.h"
#include "utils/log.h"
#include "utils/work_profile.h"

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
inline int16_t imm_se(uint32_t i) { return static_cast<int16_t>(i & 0xFFFF); }
inline uint16_t imm_ze(uint32_t i) { return static_cast<uint16_t>(i & 0xFFFF); }
inline uint32_t target(uint32_t i) { return (i & 0x03FFFFFF) << 2; }

Rsp::ImemFn special_table[64] = {};
Rsp::ImemFn primary_table[64] = {};
bool decode_tables_ready = false;
} // namespace

void Rsp::init_decode_tables() {
    if (decode_tables_ready)
        return;
    for (auto &e : special_table)
        e = &Rsp::spec_reserved;
    special_table[0x00] = &Rsp::spec_sll;
    special_table[0x02] = &Rsp::spec_srl;
    special_table[0x03] = &Rsp::spec_sra;
    special_table[0x04] = &Rsp::spec_sllv;
    special_table[0x06] = &Rsp::spec_srlv;
    special_table[0x07] = &Rsp::spec_srav;
    special_table[0x08] = &Rsp::spec_jr;
    special_table[0x09] = &Rsp::spec_jalr;
    special_table[0x0D] = &Rsp::spec_break;
    special_table[0x20] = &Rsp::spec_add;
    special_table[0x21] = &Rsp::spec_add;
    special_table[0x22] = &Rsp::spec_sub;
    special_table[0x23] = &Rsp::spec_sub;
    special_table[0x24] = &Rsp::spec_and;
    special_table[0x25] = &Rsp::spec_or;
    special_table[0x26] = &Rsp::spec_xor;
    special_table[0x27] = &Rsp::spec_nor;
    special_table[0x2A] = &Rsp::spec_slt;
    special_table[0x2B] = &Rsp::spec_sltu;

    for (auto &e : primary_table)
        e = &Rsp::op_reserved;
    primary_table[OPC_REGIMM] = &Rsp::op_regimm;
    primary_table[OPC_J] = &Rsp::op_j;
    primary_table[OPC_JAL] = &Rsp::op_jal;
    primary_table[OPC_BEQ] = &Rsp::op_beq;
    primary_table[OPC_BNE] = &Rsp::op_bne;
    primary_table[OPC_BLEZ] = &Rsp::op_blez;
    primary_table[OPC_BGTZ] = &Rsp::op_bgtz;
    primary_table[OPC_ADDI] = &Rsp::op_addi;
    primary_table[OPC_ADDIU] = &Rsp::op_addi;
    primary_table[OPC_SLTI] = &Rsp::op_slti;
    primary_table[OPC_SLTIU] = &Rsp::op_sltiu;
    primary_table[OPC_ANDI] = &Rsp::op_andi;
    primary_table[OPC_ORI] = &Rsp::op_ori;
    primary_table[OPC_XORI] = &Rsp::op_xori;
    primary_table[OPC_LUI] = &Rsp::op_lui;
    primary_table[OPC_COP0] = &Rsp::op_cop0;
    primary_table[OPC_COP2] = &Rsp::op_cop2;
    primary_table[OPC_LB] = &Rsp::op_lb;
    primary_table[OPC_LH] = &Rsp::op_lh;
    primary_table[OPC_LW] = &Rsp::op_lw;
    primary_table[OPC_LBU] = &Rsp::op_lbu;
    primary_table[OPC_LHU] = &Rsp::op_lhu;
    primary_table[OPC_SB] = &Rsp::op_sb;
    primary_table[OPC_SH] = &Rsp::op_sh;
    primary_table[OPC_SW] = &Rsp::op_sw;
    primary_table[OPC_LWC2] = &Rsp::op_lwc2;
    primary_table[OPC_SWC2] = &Rsp::op_swc2;
    decode_tables_ready = true;
}

Rsp::ImemFn Rsp::decode_opcode(uint32_t opcode) {
    init_decode_tables();
    const uint8_t primary = op(opcode);
    if (primary == OPC_SPECIAL)
        return special_table[funct(opcode)];
    return primary_table[primary];
}

void Rsp::refresh_imem_word(uint16_t addr) {
    const uint16_t a = addr & 0xFFC;
    const uint32_t opcode = Utils::read_from_byte_array32(sp_imem, a);
    imem_insns_[a >> 2] = ImemInsn{decode_opcode(opcode), opcode};
}

void Rsp::rebuild_imem_cache() {
    for (uint16_t a = 0; a < SP_IMEM_SIZE; a += 4)
        refresh_imem_word(a);
}

void Rsp::note_imem_written(uint16_t offset, uint32_t length) {
    if (length == 0)
        return;
    for (uint32_t i = 0; i < length; i += 4)
        refresh_imem_word(static_cast<uint16_t>((offset + i) & 0xFFF));
}

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
    rebuild_imem_cache();
    gpr_.fill(0);
    for (auto &v : vpr_)
        v.e.fill(0);
    acc_.h.e.fill(0);
    acc_.m.e.fill(0);
    acc_.l.e.fill(0);
    vcc_ = 0;
    vco_ = 0;
    vce_ = 0;
    divin_ = 0;
    divout_ = 0;
    divin_loaded_ = false;
    sync_point_ = false;
    broken_ = false;
    task_halted_ = false;
    running_task_ = false;
    run_after_dma_ = false;
    last_status_signals_ = 0;
    last_dpc_busy_ = 0;
    task_cycle_counter_ = 0;
}

void Rsp::set_pc(uint16_t value) {
    pc = value & 0xffc;
    next_pc = (pc + 4) & 0xffc;
    delay_slot_ = false;
}

void Rsp::branch(uint16_t target_pc) { next_pc = target_pc & 0xffc; }

void Rsp::take_break() {
    broken_ = true;
    sync_point_ = true;
}

uint64_t Rsp::run_until_sync() {
    broken_ = false;
    task_cycle_counter_ = 0;
    running_task_ = true;
    sync_point_ = false;

    constexpr uint32_t kMaxInsns = 10'000'000u;
    uint32_t ran = 0;
    while (!sync_point_ && !broken_ && !task_halted_ && ran < kMaxInsns) {
        if (status_reg.halt)
            break;
        step();
        ++ran;
        ++task_cycle_counter_;
    }
    running_task_ = false;
    WorkProfile::add_rsp_insns(ran);
    if (ran >= kMaxInsns)
        Utils::warn("RSP run_until_sync hit instruction cap");
    return (task_cycle_counter_ * 3) / 2;
}

void Rsp::do_task() {
    WorkProfile::Scoped timer(WorkProfile::Bucket::RspTask);
    sync_point_ = false;
    last_status_signals_ = 0;
    last_dpc_busy_ = 0;
    if (status_reg.dma_busy) {
        run_after_dma_ = true;
        return;
    }
    const uint64_t timer_cycles = run_until_sync();
    N64::g_scheduler().schedule_named(
        N64System::NamedEventId::Sp, timer_cycles, [] { g_rsp().on_sp_event(); });
}

void Rsp::on_sp_event() {
    if (broken_) {
        status_reg.halt = 1;
        status_reg.broke = 1;
        if (status_reg.intr_on_break) {
            g_mi().get_reg_intr().sp = 1;
            N64System::check_interrupt();
        }
        return;
    }
    if (task_halted_) {
        status_reg.halt = 1;
        task_halted_ = false;
        return;
    }
    if (status_reg.halt)
        return;
    do_task();
}

uint8_t Rsp::dmem_load8(uint32_t addr) const { return sp_dmem[addr & 0xFFF]; }

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

void Rsp::dmem_store8(uint32_t addr, uint8_t v) { sp_dmem[addr & 0xFFF] = v; }

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

void Rsp::step() {
    if (status_reg.halt)
        return;

    const ImemInsn &insn = imem_insns_[(pc & 0xFFC) >> 2];
    pc = next_pc;
    next_pc = (pc + 4) & 0xffc;
    delay_slot_ = false;

    insn.fn(*this, insn.opcode);

    if (status_reg.single_step) {
        status_reg.halt = 1;
    }
}

void Rsp::op_regimm(Rsp &r, uint32_t inst) { r.execute_regimm(inst); }
void Rsp::op_j(Rsp &r, uint32_t inst) {
    r.delay_slot_ = true;
    r.branch(static_cast<uint16_t>(target(inst)));
}
void Rsp::op_jal(Rsp &r, uint32_t inst) {
    r.delay_slot_ = true;
    r.set_gpr(31, (r.pc + 4) & 0xffc);
    r.branch(static_cast<uint16_t>(target(inst)));
}
void Rsp::op_beq(Rsp &r, uint32_t inst) {
    r.delay_slot_ = true;
    if (r.gpr(rs(inst)) == r.gpr(rt(inst)))
        r.branch(static_cast<uint16_t>(r.pc + (imm_se(inst) << 2)));
}
void Rsp::op_bne(Rsp &r, uint32_t inst) {
    r.delay_slot_ = true;
    if (r.gpr(rs(inst)) != r.gpr(rt(inst)))
        r.branch(static_cast<uint16_t>(r.pc + (imm_se(inst) << 2)));
}
void Rsp::op_blez(Rsp &r, uint32_t inst) {
    r.delay_slot_ = true;
    if (static_cast<int32_t>(r.gpr(rs(inst))) <= 0)
        r.branch(static_cast<uint16_t>(r.pc + (imm_se(inst) << 2)));
}
void Rsp::op_bgtz(Rsp &r, uint32_t inst) {
    r.delay_slot_ = true;
    if (static_cast<int32_t>(r.gpr(rs(inst))) > 0)
        r.branch(static_cast<uint16_t>(r.pc + (imm_se(inst) << 2)));
}
void Rsp::op_addi(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), r.gpr(rs(inst)) + static_cast<uint32_t>(imm_se(inst)));
}
void Rsp::op_slti(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst),
              static_cast<int32_t>(r.gpr(rs(inst))) < imm_se(inst) ? 1 : 0);
}
void Rsp::op_sltiu(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst),
              r.gpr(rs(inst)) < static_cast<uint32_t>(imm_se(inst)) ? 1 : 0);
}
void Rsp::op_andi(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), r.gpr(rs(inst)) & imm_ze(inst));
}
void Rsp::op_ori(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), r.gpr(rs(inst)) | imm_ze(inst));
}
void Rsp::op_xori(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), r.gpr(rs(inst)) ^ imm_ze(inst));
}
void Rsp::op_lui(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), static_cast<uint32_t>(imm_ze(inst)) << 16);
}
void Rsp::op_cop0(Rsp &r, uint32_t inst) { r.execute_cop0(inst); }
void Rsp::op_cop2(Rsp &r, uint32_t inst) { r.execute_cop2(inst); }
void Rsp::op_lb(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), static_cast<int32_t>(static_cast<int8_t>(
                            r.dmem_load8(r.gpr(rs(inst)) + imm_se(inst)))));
}
void Rsp::op_lbu(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), r.dmem_load8(r.gpr(rs(inst)) + imm_se(inst)));
}
void Rsp::op_lh(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), static_cast<int32_t>(static_cast<int16_t>(
                            r.dmem_load16(r.gpr(rs(inst)) + imm_se(inst)))));
}
void Rsp::op_lhu(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), r.dmem_load16(r.gpr(rs(inst)) + imm_se(inst)));
}
void Rsp::op_lw(Rsp &r, uint32_t inst) {
    r.set_gpr(rt(inst), r.dmem_load32(r.gpr(rs(inst)) + imm_se(inst)));
}
void Rsp::op_sb(Rsp &r, uint32_t inst) {
    r.dmem_store8(r.gpr(rs(inst)) + imm_se(inst),
                  static_cast<uint8_t>(r.gpr(rt(inst))));
}
void Rsp::op_sh(Rsp &r, uint32_t inst) {
    r.dmem_store16(r.gpr(rs(inst)) + imm_se(inst),
                   static_cast<uint16_t>(r.gpr(rt(inst))));
}
void Rsp::op_sw(Rsp &r, uint32_t inst) {
    r.dmem_store32(r.gpr(rs(inst)) + imm_se(inst), r.gpr(rt(inst)));
}
void Rsp::op_lwc2(Rsp &r, uint32_t inst) { r.execute_lwc2(inst); }
void Rsp::op_swc2(Rsp &r, uint32_t inst) { r.execute_swc2(inst); }
void Rsp::op_reserved(Rsp &r, uint32_t inst) {
    Utils::warn("RSP unknown opcode {:#04x} inst={:#010x} pc={:#05x}", op(inst),
                inst, (r.pc - 4) & 0xffc);
}

void Rsp::spec_sll(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rt(inst)) << sa(inst));
}
void Rsp::spec_srl(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rt(inst)) >> sa(inst));
}
void Rsp::spec_sra(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst),
              static_cast<uint32_t>(static_cast<int32_t>(r.gpr(rt(inst))) >>
                                    sa(inst)));
}
void Rsp::spec_sllv(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rt(inst)) << (r.gpr(rs(inst)) & 31));
}
void Rsp::spec_srlv(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rt(inst)) >> (r.gpr(rs(inst)) & 31));
}
void Rsp::spec_srav(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst),
              static_cast<uint32_t>(static_cast<int32_t>(r.gpr(rt(inst))) >>
                                    (r.gpr(rs(inst)) & 31)));
}
void Rsp::spec_jr(Rsp &r, uint32_t inst) {
    r.delay_slot_ = true;
    r.branch(static_cast<uint16_t>(r.gpr(rs(inst))));
}
void Rsp::spec_jalr(Rsp &r, uint32_t inst) {
    r.delay_slot_ = true;
    const uint32_t link = (r.pc + 4) & 0xffc;
    r.branch(static_cast<uint16_t>(r.gpr(rs(inst))));
    r.set_gpr(rd(inst), link);
}
void Rsp::spec_break(Rsp &r, uint32_t) { r.take_break(); }
void Rsp::spec_add(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rs(inst)) + r.gpr(rt(inst)));
}
void Rsp::spec_sub(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rs(inst)) - r.gpr(rt(inst)));
}
void Rsp::spec_and(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rs(inst)) & r.gpr(rt(inst)));
}
void Rsp::spec_or(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rs(inst)) | r.gpr(rt(inst)));
}
void Rsp::spec_xor(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rs(inst)) ^ r.gpr(rt(inst)));
}
void Rsp::spec_nor(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), ~(r.gpr(rs(inst)) | r.gpr(rt(inst))));
}
void Rsp::spec_slt(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), static_cast<int32_t>(r.gpr(rs(inst))) <
                                static_cast<int32_t>(r.gpr(rt(inst)))
                            ? 1
                            : 0);
}
void Rsp::spec_sltu(Rsp &r, uint32_t inst) {
    r.set_gpr(rd(inst), r.gpr(rs(inst)) < r.gpr(rt(inst)) ? 1 : 0);
}
void Rsp::spec_reserved(Rsp &, uint32_t inst) {
    Utils::warn("RSP SPECIAL funct={:#04x}", funct(inst));
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
    case 4: {
        constexpr uint32_t kSigMask = 0x7F80u;
        const uint32_t signals = status_reg.raw & kSigMask;
        if (running_task_ && signals == last_status_signals_ && signals != 0)
            sync_point_ = true;
        last_status_signals_ = signals;
        return status_reg.raw;
    }
    case 5:
        return status_reg.dma_full;
    case 6:
        return status_reg.dma_busy;
    case 7: {
        if (semaphore_held) {
            if (running_task_)
                sync_point_ = true;
            return 1;
        }
        semaphore_held = true;
        return 0;
    }
    case 8:
        return g_dpc().get_start();
    case 9:
        return g_dpc().get_end();
    case 10:
        return g_dpc().get_current();
    case 11: {
        const uint32_t raw = g_dpc().get_status().raw;
        constexpr uint32_t kBusyMask = (1u << 5) | (1u << 6); // pipe|cmd busy
        const uint32_t busy = raw & kBusyMask;
        if (running_task_ && busy == last_dpc_busy_ && busy != 0)
            sync_point_ = true;
        last_dpc_busy_ = busy;
        return raw;
    }
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
        if (running_task_)
            sync_point_ = true;
        dma_read();
        break;
    case 3:
        dma.raw = value;
        if (value == 0xffffffffu) {
            dma.raw = 0xFF8;
            break;
        }
        if (running_task_)
            sync_point_ = true;
        dma_write();
        break;
    case 4: {
        status_reg_write(value);
        if ((value & 0x2u) != 0 && (value & 0x1u) == 0) {
            status_reg.halt = 0;
            task_halted_ = true;
            sync_point_ = true;
        }
    } break;
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
    vu_profile_cop2_move(sub);
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
        // rd&3==2 and ==3 both map to VCE (hardware alias).
        uint32_t val = 0;
        switch (vd & 3) {
        case 0:
            val = vco_;
            break;
        case 1:
            val = vcc_;
            break;
        case 2:
        case 3:
            val = vce_;
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
        // rd&3==2 and ==3 both map to VCE (hardware alias).
        const uint16_t val = static_cast<uint16_t>(gpr(vt));
        switch (vd & 3) {
        case 0:
            vco_ = val;
            break;
        case 1:
            vcc_ = val;
            break;
        case 2:
        case 3:
            vce_ = static_cast<uint8_t>(val);
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

    const uint32_t check_len =
        dma.count == 0 ? length
                       : (dma.count + 1) * length + dma.count * dma.skip;
    Rdp::check_framebuffers(dram_address, check_len);

    for (uint32_t i = 0; i < dma.count + 1; i++) {
        const uint32_t row_mem = mem_address;
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
        if (to_imem)
            note_imem_written(static_cast<uint16_t>(row_mem), length);
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

    const uint32_t check_len =
        dma.count == 0 ? length
                       : (dma.count + 1) * length + dma.count * dma.skip;
    Rdp::on_rdram_write(dram_address, check_len);

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
    maybe_invalidate_code(shadow_dram_addr.address & RSP_DRAM_ADDR_MASK,
                          (dma.count + 1) * (length + dma.skip));
    dram_addr.address = dram_address;
    mem_addr.address = mem_address;
    mem_addr.imem = from_imem;
    dma.raw = 0xFF8 | (dma.skip << 20);
}

void Rsp::status_reg_write(uint32_t value) {
    sp_status_write_t write;
    write.raw = value;
    const bool was_halted = status_reg.halt != 0;

    if (write.clear_halt && !write.set_halt)
        status_reg.halt = 0;
    if (!write.clear_halt && write.set_halt) {
        N64::g_scheduler().cancel_named(N64System::NamedEventId::Sp);
        status_reg.halt = 1;
    }
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

    if (!status_reg.halt && was_halted) {
        broken_ = false;
        task_halted_ = false;
        g_debugger().on_rsp_unhalt();
        do_task();
    }
}

Rsp Rsp::instance{};

} // namespace Rsp

Rsp::Rsp &g_rsp() { return Rsp::Rsp::get_instance(); }

} // namespace N64

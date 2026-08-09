#include "cpu/cpu.h"
#include "cpu/cached_interp.h"
#include "cpu/idle_skip.h"
#include "debugger/debugger.h"
#include "mmu/tlb.h"
#include "n64_system/interrupt.h"
#include "utils/log.h"

namespace N64 {
namespace Cpu {

uint64_t Gpr::read(uint32_t reg_num) const {
    assert(reg_num < 32);
    if (reg_num == 0) {
        return 0;
    } else {
        return reg[reg_num];
    }
}

void Gpr::write(uint32_t reg_num, uint64_t value) {
    assert(reg_num < 32);
    if (reg_num != 0) {
        reg[reg_num] = value;
    }
}

Cpu Cpu::instance{};

void Cpu::reset() {
    Utils::debug("Resetting CPU");
    delay_slot = false;
    prev_delay_slot = false;
    cop0.reset();
    cop1.reset();

    prev_pc = 0;
    pc = 0;
    next_pc = 4;
}

void Cpu::dump() {
    Utils::info("======= Core dump =======");
    Utils::info("PC\t= {:#x}", pc);
    Utils::info("prevPC\t= {:#018x}\tnextPC\t= {:#018x}", prev_pc, next_pc);
    Utils::info("hi\t= {:#018x}\tlo\t= {:#018x}", hi, lo);
    for (int i = 0; i < 16; i++) {
        Utils::info("{}\t= {:#018x}\t{}\t= {:#018x}", GPR_NAMES[i], gpr.read(i),
                    GPR_NAMES[i + 16], gpr.read(i + 16));
    }
    Utils::info("");
    cop0.dump();
    cop1.dump();
    Utils::info("=========================");
}

void Cpu::set_pc64(uint64_t value) {
    prev_pc = pc;
    pc = value;
    next_pc = value + 4;
}

void Cpu::set_pc32(uint32_t value) {
    prev_pc = pc;
    // Keep pc and next_pc consistently sign-extended from 32-bit addresses.
    pc = static_cast<uint64_t>(static_cast<int32_t>(value));
    next_pc = pc + 4;
}

void Cpu::add_count(uint32_t n) {
    if (n == 0)
        return;
    const uint64_t before = cop0.reg.count;
    cop0.reg.count = (before + n) & 0x1FFFFFFFFULL;
    // Timer interrupt when the internal (PClock) counter reaches Compare<<1.
    // Detect crossing so JIT multi-cycle blocks cannot miss the edge.
    const uint64_t target =
        (static_cast<uint64_t>(cop0.reg.compare) << 1) & 0x1FFFFFFFFULL;
    const uint64_t dist = (target - before) & 0x1FFFFFFFFULL;
    if (dist <= static_cast<uint64_t>(n)) {
        cop0.reg.cause.ip7 = true;
        N64System::check_interrupt();
    }
}

uint64_t Cpu::get_pc64() const { return pc; }

void Cpu::step() { CachedInterp::step_one(); }

static bool is_xtlb_miss(uint64_t bad_vaddr, cop0_status_t status) {
    // Assume 64bit addressing mode.
    switch ((bad_vaddr >> 62) & 3) {
    case 0b00: // user
        return status.ux;
    case 0b01: // supervisor
        return status.sx;
    case 0b11: // kernel
        return status.kx;
    default:
        Utils::critical("BadVaddr >> 62 == 0b10");
        Utils::abort("Aborted");
    }
}

void Cpu::execute_instruction(instruction_t inst) {
    CachedInterp::decode(inst)(*this, inst);
}

void Cpu::branch_likely_addr64(Cpu &cpu, bool cond, uint64_t vaddr) {
    // 分岐成立時のみ遅延スロットを実行する
    cpu.delay_slot = true; // FIXME: correct?
    if (cond) {
        // Utils::trace("branch likely taken");
        cpu.next_pc = vaddr;
        idle_skip_check_absolute(cpu, vaddr);
    } else {
        // Utils::trace("branch likely not taken");
        cpu.set_pc64(cpu.pc + 4);
    }
}

void Cpu::branch_addr64(Cpu &cpu, bool cond, uint64_t vaddr) {
    cpu.delay_slot = true;
    if (cond) {
        // Utils::trace("branch taken");
        cpu.next_pc = vaddr;
        idle_skip_check_absolute(cpu, vaddr);
    } else {
        // Utils::trace("branch not taken");
    }
}

void Cpu::branch_likely_offset16(Cpu &cpu, bool cond, instruction_t inst) {
    int64_t offset = (int16_t)inst.i_type.imm; // sext
    // 負数の左シフトはUBなので乗算で実装
    offset *= 4;
    // Utils::trace("pc <= pc {:+#x}?", (int64_t)offset);
    branch_likely_addr64(cpu, cond, cpu.pc + offset);
}

void Cpu::branch_offset16(Cpu &cpu, bool cond, instruction_t inst) {
    int64_t offset = (int16_t)inst.i_type.imm; // sext
    // 負数の左シフトはUBなので乗算で実装
    offset *= 4;
    // Utils::trace("pc <= pc {:+#x}?", (int64_t)offset);
    branch_addr64(cpu, cond, cpu.pc + offset);
}

void Cpu::link(Cpu &cpu, uint8_t reg) { cpu.gpr.write(reg, cpu.pc + 4); }

// https://github.com/SimoneN64/Kaizen/blob/74dccb6ac6a679acbf41b497151e08af6302b0e9/src/backend/core/registers/Cop0.cpp#L253
void Cpu::handle_exception(ExceptionCode exception_code,
                           uint8_t coprocessor_error, bool use_prev_pc) {
    bool old_exl = cop0.reg.status.exl;
    int64_t epc = use_prev_pc ? prev_pc : pc;

    if (cop0.reg.status.exl == 0) {
        if (prev_delay_slot) {
            cop0.reg.cause.branch_delay = 1;
            // FIXME: Is just minus 4 fine?
            epc -= 4;
        } else {
            cop0.reg.cause.branch_delay = 0;
        }
        cop0.reg.status.exl = 1;
        cop0.reg.epc = epc;
    }

    cop0.reg.cause.coprocessor_error = coprocessor_error;
    cop0.reg.cause.exception_code = static_cast<uint8_t>(exception_code);

    if (cop0.reg.status.bev == 1) {
        Utils::unimplemented("BEV is set");
    }

    uint32_t vector = 0x80000180;
    switch (exception_code) {
    case ExceptionCode::INTERRUPT:            // fallthrough
    case ExceptionCode::TLB_MODIFICATION:     // fallthrough
    case ExceptionCode::ADDRESS_ERROR_LOAD:   // fallthrough
    case ExceptionCode::ADDRESS_ERROR_STORE:  // fallthrough
    case ExceptionCode::BUS_ERROR_INS_FETCH:  // fallthrough
    case ExceptionCode::BUS_ERROR_LOAD_STORE: // fallthrough
    case ExceptionCode::SYSCALL:              // fallthrough
    case ExceptionCode::BREAKPOINT:           // fallthrough
    case ExceptionCode::RESERVED_INSTR:       // fallthrough
    case ExceptionCode::COPROCESSOR_UNUSABLE: // fallthrough
    case ExceptionCode::ARITHMETIC_OVERFLOW:  // fallthrough
    case ExceptionCode::TRAP:                 // fallthrough
    case ExceptionCode::FLOATING_POINT:       // fallthrough
    case ExceptionCode::WATCH: {
        vector = 0x80000180;
    } break;
    case ExceptionCode::TLB_MISS_LOAD: // fallthrough
    case ExceptionCode::TLB_MISS_STORE: {
        if (old_exl || g_tlb().get_last_error() == Mmu::TLBError::INVALID) {
            vector = 0x80000180;
        } else if (is_xtlb_miss(cop0.reg.bad_vaddr, cop0.reg.status)) {
            vector = 0x80000080;
        } else {
            vector = 0x80000000;
        }
    } break;
    default: {
        Utils::critical("Unimplemented. exception code = {}",
                        static_cast<uint8_t>(exception_code));
        Utils::abort("Aborted");
    } break;
    }

    set_pc32(vector);
    g_debugger().on_exception(exception_code, vector);
}

} // namespace Cpu

Cpu::Cpu &g_cpu() { return Cpu::Cpu::get_instance(); }

} // namespace N64

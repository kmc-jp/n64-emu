﻿#ifndef CPU_INSTRUCTIONS_CPP
#define CPU_INSTRUCTIONS_CPP

#include "cop0.h"
#include "cpu.h"
#include "gpr.h"
#include "instruction.h"
#include "memory/bus.h"
#include "memory/tlb.h"
#include "mmu/mmu.h"
#include "utils/utils.h"
#include <cstdint>
#include <optional>

namespace N64 {
namespace Cpu {

constexpr uint8_t RA = 31;

class Cpu::CpuImpl {
  public:
    static void branch_likely_addr64(Cpu &cpu, bool cond, uint64_t vaddr) {
        // 分岐成立時のみ遅延スロットを実行する
        cpu.delay_slot = true; // FIXME: correct?
        if (cond) {
            Utils::trace("branch likely taken");
            cpu.next_pc = vaddr;
        } else {
            Utils::trace("branch likely not taken");
            cpu.set_pc64(cpu.pc + 4);
        }
    }

    static void branch_addr64(Cpu &cpu, bool cond, uint64_t vaddr) {
        cpu.delay_slot = true;
        if (cond) {
            Utils::trace("branch taken");
            cpu.next_pc = vaddr;
        } else {
            Utils::trace("branch not taken");
        }
    }

    static void branch_likely_offset16(Cpu &cpu, bool cond,
                                       instruction_t inst) {
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        // 負数の左シフトはUBなので乗算で実装
        offset *= 4;
        Utils::trace("pc <= pc {:+#x}?", (int64_t)offset);
        branch_likely_addr64(cpu, cond, cpu.pc + offset);
    }

    static void branch_offset16(Cpu &cpu, bool cond, instruction_t inst) {
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        // 負数の左シフトはUBなので乗算で実装
        offset *= 4;
        Utils::trace("pc <= pc {:+#x}?", (int64_t)offset);
        branch_addr64(cpu, cond, cpu.pc + offset);
    }

    static void link(Cpu &cpu, uint8_t reg) { cpu.gpr.write(reg, cpu.pc + 4); }

    static void op_add(Cpu &cpu, instruction_t inst) {
        // TODO: throw exception
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L903
        assert_encoding_is_valid(inst.r_type.sa == 0);
        uint32_t rs = cpu.gpr.read(inst.r_type.rs);
        uint32_t rt = cpu.gpr.read(inst.r_type.rt);
        uint32_t res = rs + rt;
        Utils::trace("ADD: {} <= {} + {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, (int64_t)((int32_t)res));
    }

    static void op_addu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L915
        assert_encoding_is_valid(inst.r_type.sa == 0);
        uint32_t rs = cpu.gpr.read(inst.r_type.rs);
        uint32_t rt = cpu.gpr.read(inst.r_type.rt);
        int32_t res = rs + rt;
        Utils::trace("ADDU: {} <= {} + {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_sub(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L932
        // TODO: throw exception
        assert_encoding_is_valid(inst.r_type.sa == 0);
        int32_t rs = cpu.gpr.read(inst.r_type.rs);
        int32_t rt = cpu.gpr.read(inst.r_type.rt);
        int32_t res = rs - rt;
        Utils::trace("SUB: {} <= {} - {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_subu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L946
        assert_encoding_is_valid(inst.r_type.sa == 0);
        uint32_t rs = cpu.gpr.read(inst.r_type.rs);
        uint32_t rt = cpu.gpr.read(inst.r_type.rt);
        int32_t res = rs - rt;
        Utils::trace("SUBU: {} <= {} - {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_mult(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L780
        assert_encoding_is_valid(inst.r_type.sa == 0);
        int32_t rs = cpu.gpr.read(inst.r_type.rs); // as signed 32bit
        int32_t rt = cpu.gpr.read(inst.r_type.rt); // as signed 32bit
        int64_t d_rs = rs;                         // sext
        int64_t d_rt = rt;                         // sext
        int64_t res = d_rs * d_rt;
        int32_t lo = res & 0xFFFFFFFF;
        int32_t hi = (res >> 32) & 0xFFFFFFFF;
        int64_t sext_lo = lo; // sext
        int64_t sext_hi = hi; // sext
        Utils::trace("MULT {}, {}", GPR_NAMES[inst.r_type.rs],
                     GPR_NAMES[inst.r_type.rt]);
        cpu.lo = sext_lo;
        cpu.hi = sext_hi;
    }

    static void op_multu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L796
        assert_encoding_is_valid(inst.r_type.sa == 0);
        uint64_t rs = cpu.gpr.read(inst.r_type.rs) & 0xFFFFFFFF;
        uint64_t rt = cpu.gpr.read(inst.r_type.rt) & 0xFFFFFFFF;
        uint64_t res = rs * rt;
        int32_t lo = res & 0xFFFFFFFF;
        int32_t hi = (res >> 32) & 0xFFFFFFFF;
        int64_t sext_lo = lo; // sext
        int64_t sext_hi = hi; // sext
        Utils::trace("MULTU {}, {}", GPR_NAMES[inst.r_type.rs],
                     GPR_NAMES[inst.r_type.rt]);
        cpu.lo = sext_lo;
        cpu.hi = sext_hi;
    }

    static void op_div(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L809
        int64_t dividend = (int32_t)cpu.gpr.read(inst.r_type.rs);
        int64_t divisor = (int32_t)cpu.gpr.read(inst.r_type.rt);
        Utils::trace("DIV {}, {}", GPR_NAMES[inst.r_type.rs],
                     GPR_NAMES[inst.r_type.rt]);
        if (divisor == 0) {
            Utils::warn("division by zero");
            cpu.hi = dividend;
            if (dividend >= 0)
                cpu.lo = (int64_t)-1;
            else
                cpu.lo = (int64_t)1;
        } else {
            int32_t quotient = dividend / divisor;
            int32_t remainder = dividend % divisor;

            cpu.lo = quotient;
            cpu.hi = remainder;
        }
    }

    static void op_divu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L831
        uint32_t dividend = cpu.gpr.read(inst.r_type.rs);
        uint32_t divisor = cpu.gpr.read(inst.r_type.rt);
        Utils::trace("DIVU {}, {}", GPR_NAMES[inst.r_type.rs],
                     GPR_NAMES[inst.r_type.rt]);

        if (divisor == 0) {
            Utils::warn("division by zero");
            cpu.hi = (int32_t)dividend;
            cpu.lo = 0xFFFF'FFFF'FFFF'FFFF;
        } else {
            int32_t quotient = dividend / divisor;
            int32_t remainder = dividend % divisor;

            cpu.lo = quotient;
            cpu.hi = remainder;
        }
    }

    static void op_sll(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L698
        assert_encoding_is_valid(inst.r_type.rs == 0);
        int32_t res = cpu.gpr.read(inst.r_type.rt) << inst.r_type.sa;
        if (inst.raw == 0) {
            Utils::trace("NOP");
        } else {
            Utils::trace("SLL: {} <= {} << {}", GPR_NAMES[inst.r_type.rd],
                         GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
        }
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_srl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L703
        uint32_t value = cpu.gpr.read(inst.r_type.rt);
        int32_t res = value >> inst.r_type.sa;
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
        Utils::trace("SRL {} {} {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_sra(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L709
        int64_t value = cpu.gpr.read(inst.r_type.rt);
        int32_t res = (int64_t)(value >> (uint64_t)inst.r_type.sa);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
        Utils::trace("SRA {} {} {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_srav(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L715
        int64_t value = cpu.gpr.read(inst.r_type.rt);
        int32_t res =
            (int64_t)(value >> (cpu.gpr.read(inst.r_type.rs) & 0b11111));
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
        Utils::trace("SRAV {}, {}, {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rt], GPR_NAMES[inst.r_type.rs]);
    }

    static void op_sllv(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L721
        assert_encoding_is_valid(inst.r_type.sa == 0);
        Utils::trace("SLLV {}, {}, {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rt], GPR_NAMES[inst.r_type.rs]);
        uint32_t value = cpu.gpr.read(inst.r_type.rt); // as 32bit
        int32_t res = value << (cpu.gpr.read(inst.r_type.rs) & 0b11111);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_srlv(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L727
        assert_encoding_is_valid(inst.r_type.sa == 0);
        Utils::trace("SRLV {}, {}, {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rt], GPR_NAMES[inst.r_type.rs]);
        uint32_t value = cpu.gpr.read(inst.r_type.rt); // as 32bit
        int32_t res = value >> (cpu.gpr.read(inst.r_type.rs) & 0b11111);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_slt(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L962
        int64_t rs = cpu.gpr.read(inst.r_type.rs);
        int64_t rt = cpu.gpr.read(inst.r_type.rt);
        Utils::trace("SLT {} {} {} ", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, rs < rt ? 1 : 0);
    }

    static void op_sltu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L968
        assert_encoding_is_valid(inst.r_type.sa == 0);
        uint64_t rs = cpu.gpr.read(inst.r_type.rs); // unsigned
        uint64_t rt = cpu.gpr.read(inst.r_type.rt); // unsigned
        Utils::trace("SLTU {} {} {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, rs < rt ? 1 : 0);
    }

    static void op_slti(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L199
        int16_t imm = inst.i_type.imm;
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        Utils::trace("SLTI {} {} {}", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, rs < imm ? 1 : 0);
    }

    static void op_sltiu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L210
        int16_t imm = inst.i_type.imm;
        uint64_t rs = cpu.gpr.read(inst.i_type.rs);
        Utils::trace("SLTIU {} {} {}", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, rs < imm ? 1 : 0);
    }

    static void op_and(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L922
        assert_encoding_is_valid(inst.r_type.sa == 0);
        Utils::trace("AND: {} <= {} & {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, cpu.gpr.read(inst.r_type.rs) &
                                          cpu.gpr.read(inst.r_type.rt));
    }

    static void op_or(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L954
        assert_encoding_is_valid(inst.r_type.sa == 0);
        Utils::trace("OR: {} <= {} | {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, cpu.gpr.read(inst.r_type.rs) |
                                          cpu.gpr.read(inst.r_type.rt));
    }

    static void op_xor(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L958
        assert_encoding_is_valid(inst.r_type.sa == 0);
        Utils::trace("XOR: {} <= {} ^ {}", GPR_NAMES[inst.r_type.rd],
                     GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, cpu.gpr.read(inst.r_type.rs) ^
                                          cpu.gpr.read(inst.r_type.rt));
    }

    static void op_jr(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L733
        assert_encoding_is_valid(inst.r_type.rt == 0 && inst.r_type.rd == 0 &&
                                 inst.r_type.sa == 0);
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        Utils::trace("JR {}", GPR_NAMES[inst.r_type.rs]);
        branch_addr64(cpu, true, rs);
    }

    static void op_jalr(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L737
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        branch_addr64(cpu, true, rs);
        link(cpu, inst.r_type.rd);
        Utils::trace("JALR addr={} dest={}", GPR_NAMES[inst.r_type.rs],
                     GPR_NAMES[inst.r_type.rd]);
    }

    static void op_mfhi(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L746
        assert_encoding_is_valid(inst.r_type.rs == 0 && inst.r_type.rt == 0 &&
                                 inst.r_type.sa == 0);
        Utils::trace("MFHI {} <= hi", GPR_NAMES[inst.r_type.rd]);
        cpu.gpr.write(inst.r_type.rd, cpu.hi);
    }

    static void op_mflo(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L754
        assert_encoding_is_valid(inst.r_type.rs == 0 && inst.r_type.rt == 0 &&
                                 inst.r_type.sa == 0);
        Utils::trace("MFLO {} <= lo", GPR_NAMES[inst.r_type.rd]);
        cpu.gpr.write(inst.r_type.rd, cpu.lo);
    }

    static void op_bltz(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1113
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        Utils::trace("BLTZ {}", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs < 0, inst);
    }

    static void op_bltzl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1118
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        Utils::trace("BLTZL {}", GPR_NAMES[inst.i_type.rs]);
        branch_likely_offset16(cpu, rs < 0, inst);
    }

    static void op_bgez(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1123
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        Utils::trace("BGEZ {}", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs >= 0, inst);
    }

    static void op_bgezl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1128
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        Utils::trace("BGEZL {}", GPR_NAMES[inst.i_type.rs]);
        branch_likely_offset16(cpu, rs >= 0, inst);
    }

    static void op_bltzal(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1133
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        Utils::trace("BLTZAL cond: {} < 0", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs < 0, inst);
        link(cpu, RA);
    }

    static void op_bgezal(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1139
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        Utils::trace("BGEZAL cond: {} >= 0", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs >= 0, inst);
        link(cpu, RA);
    }

    static void op_j(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L181
        uint64_t target = inst.j_type.target;
        target <<= 2;
        target |= ((cpu.pc - 4) & 0xFFFFFFFF'F0000000); // pc is now 4 ahead
        Utils::trace("J {:#x}", target);
        branch_addr64(cpu, true, target);
    }

    static void op_jal(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L189
        link(cpu, RA);
        uint64_t target = inst.j_type.target;
        target <<= 2;
        target |= ((cpu.pc - 4) & 0xFFFFFFFF'F0000000); // pc is now 4 ahead
        Utils::trace("JAL {:#x}", target);
        branch_addr64(cpu, true, target);
    }

    static void op_lui(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L259
        assert_encoding_is_valid(inst.i_type.rs == 0);
        int64_t simm = (int16_t)inst.i_type.imm; // sext
        // 負数の左シフトはUBなので乗算で実装
        simm *= 65536;
        Utils::trace("LUI: {} <= {:#x}", GPR_NAMES[inst.i_type.rt], simm);
        cpu.gpr.write(inst.i_type.rt, simm);
    }

    static void op_lw(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L317
        int16_t offset = inst.i_type.imm;
        Utils::trace("LW: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], offset);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            int32_t word = Memory::read_paddr32(paddr.value());
            cpu.gpr.write(inst.i_type.rt, (int64_t)word); // sext
        } else {
            g_tlb().generate_exception(vaddr);
        }
    }

    static void op_lwu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L336
        int16_t offset = inst.i_type.imm;
        Utils::trace("LWU: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], offset);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint32_t word = Memory::read_paddr32(paddr.value());
            cpu.gpr.write(inst.i_type.rt, (uint64_t)word); // zext
        } else {
            g_tlb().generate_exception(vaddr);
        }
    }

    static void op_lhu(Cpu &cpu, instruction_t inst) {
        // https://hack64.net/docs/VR43XX.pdf p.451
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L282
        int16_t offset = inst.i_type.imm;
        Utils::trace("LHU: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], offset);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            const uint16_t value = Memory::read_paddr16(paddr.value());
            cpu.gpr.write(inst.i_type.rt, static_cast<uint64_t>(value)); // zext

        } else {
            g_tlb().generate_exception(vaddr);
        }
    }

    static void op_ld(Cpu &cpu, instruction_t inst) {
        // https://hack64.net/docs/VR43XX.pdf p.441
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L282
        int16_t offset = inst.i_type.imm;
        Utils::trace("LD: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], offset);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint64_t value = Memory::read_paddr64(paddr.value());
            cpu.gpr.write(inst.i_type.rt, value);
        } else {
            g_tlb().generate_exception(vaddr);
        }
    }

    static void op_sw(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L382
        int16_t offset = inst.i_type.imm;
        Utils::trace("SW: *({} + {:#x}) <= {}", GPR_NAMES[inst.i_type.rs],
                     offset, GPR_NAMES[inst.r_type.rt]);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs);
        vaddr += offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint32_t word = cpu.gpr.read(inst.r_type.rt);
            Memory::write_paddr32(paddr.value(), word);
        } else {
            g_tlb().generate_exception(vaddr);
        }
    }

    static void op_sd(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L402
        int16_t offset = inst.i_type.imm;
        Utils::trace("SD: *({} + {:#x}) <= {}", GPR_NAMES[inst.i_type.rs],
                     offset, GPR_NAMES[inst.r_type.rt]);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs);
        vaddr += offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint64_t dword = cpu.gpr.read(inst.r_type.rt);
            Memory::write_paddr64(paddr.value(), dword);
        } else {
            g_tlb().generate_exception(vaddr);
        }
    }

    static void op_addi(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L99
        uint32_t rs = cpu.gpr.read(inst.i_type.rs);
        uint32_t addend = (int32_t)((int16_t)inst.i_type.imm); // sext
        uint32_t res = rs + addend;
        Utils::trace("ADDI: {} <= {} + {:#x}", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], addend);
        // TODO: check overflow
        cpu.gpr.write(inst.i_type.rt, (int64_t)((int32_t)res));
    }

    static void op_addiu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L110
        uint32_t rs = cpu.gpr.read(inst.i_type.rs);
        int16_t addend = inst.i_type.imm; // sext
        int32_t res = rs + addend;
        Utils::trace("ADDIU: {} <= {} + {:#x}", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], addend);
        cpu.gpr.write(inst.i_type.rt, (int64_t)res);
    }

    static void op_daddi(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L110
        uint64_t addend = (int64_t)((int16_t)inst.i_type.imm);
        uint64_t rs = cpu.gpr.read(inst.i_type.rs);
        uint64_t res = rs + addend;
        // TODO: check overflow
        Utils::trace("DADDI: {} <= {} + {:#x}", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], addend);
        cpu.gpr.write(inst.i_type.rt, res);
    }

    static void op_andi(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L131
        uint64_t imm = inst.i_type.imm; // zext
        Utils::trace("ANDI: {} <= {} & {:#x}", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, cpu.gpr.read(inst.i_type.rs) & imm);
    }

    static void op_ori(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L422
        uint64_t imm = inst.i_type.imm; // zext
        Utils::trace("ORI: {} <= {} | {:#x}", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, cpu.gpr.read(inst.i_type.rs) | imm);
    }

    static void op_xori(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L426
        uint64_t imm = inst.i_type.imm; // zext
        Utils::trace("XORI: {} <= {} ^ {:#x}", GPR_NAMES[inst.i_type.rt],
                     GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, cpu.gpr.read(inst.i_type.rs) ^ imm);
    }

    static void op_beq(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L137
        Utils::trace("BEQ: cond {} == {}", GPR_NAMES[inst.i_type.rs],
                     GPR_NAMES[inst.i_type.rt]);
        branch_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) == cpu.gpr.read(inst.i_type.rt),
            inst);
    }

    static void op_beql(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L141
        Utils::trace("BEQL: cond {} == {}", GPR_NAMES[inst.i_type.rs],
                     GPR_NAMES[inst.i_type.rt]);
        branch_likely_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) == cpu.gpr.read(inst.i_type.rt),
            inst);
    }

    static void op_bne(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L165
        Utils::trace("BNE: cond {} != {}", GPR_NAMES[inst.i_type.rs],
                     GPR_NAMES[inst.i_type.rt]);
        Utils::trace("{} : {:#x}, {} = {:#x}", GPR_NAMES[inst.i_type.rs],
                     cpu.gpr.read(inst.i_type.rs), GPR_NAMES[inst.i_type.rt],
                     cpu.gpr.read(inst.i_type.rt));
        branch_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) != cpu.gpr.read(inst.i_type.rt),
            inst);
    }

    static void op_bnel(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L169
        Utils::trace("BNEL: cond {} != {}", GPR_NAMES[inst.i_type.rs],
                     GPR_NAMES[inst.i_type.rt]);
        branch_likely_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) != cpu.gpr.read(inst.i_type.rt),
            inst);
    }

    static void op_blez(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L155
        assert_encoding_is_valid(inst.i_type.rt == 0);
        int64_t rs = cpu.gpr.read(inst.i_type.rs); // as signed integer
        Utils::trace("BLEZ: cond {} <= 0", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs <= 0, inst);
    }

    static void op_blezl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L160
        assert_encoding_is_valid(inst.i_type.rt == 0);
        int64_t rs = cpu.gpr.read(inst.i_type.rs); // as signed integer
        Utils::trace("BLEZL: cond {} <= 0", GPR_NAMES[inst.i_type.rs]);
        branch_likely_offset16(cpu, rs <= 0, inst);
    }

    static void op_bgtz(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L145
        assert_encoding_is_valid(inst.i_type.rt == 0);
        int64_t rs = cpu.gpr.read(inst.i_type.rs); // as signed integer
        Utils::trace("BGTZ: cond {} > 0", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs > 0, inst);
    }

    static void op_bgtzl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L150
        assert_encoding_is_valid(inst.i_type.rt == 0);
        int64_t rs = cpu.gpr.read(inst.i_type.rs); // as signed integer
        Utils::trace("BGTZL: cond {} > 0", GPR_NAMES[inst.i_type.rs]);
        branch_likely_offset16(cpu, rs > 0, inst);
    }

    static void op_cache() {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L177
        // B.1.1 CACHE Instruction
        // https://hack64.net/docs/VR43XX.pdf
        // no need for emulation?
        Utils::trace("CACHE: no effect");
    }

    static void op_mfc0(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L220
        Utils::trace("MFC0: {} <= COP0.reg[{}]", GPR_NAMES[inst.copz_type1.rt],
                     static_cast<uint32_t>(inst.copz_type1.rd));
        int32_t tmp = cpu.cop0.reg.read(inst.copz_type1.rd);
        int64_t stmp = tmp; // sext
        // FIXME: T+1 (delay)
        cpu.gpr.write(inst.copz_type1.rt, stmp);
    }

    static void op_mtc0(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L225
        Utils::trace("MTC0: COP0.reg[{}] <= {}",
                     static_cast<uint32_t>(inst.copz_type1.rd),
                     GPR_NAMES[inst.copz_type1.rt]);
        uint32_t tmp = cpu.gpr.read(inst.copz_type1.rt);
        // FIXME: T+1 (delay)
        cpu.cop0.reg.write(inst.copz_type1.rd, tmp);
    }

    static void op_dmfc0(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L230
        Utils::trace("DMFC0: {} <= COP0.reg[{}]", GPR_NAMES[inst.copz_type1.rt],
                     static_cast<uint32_t>(inst.copz_type1.rd));
        uint64_t tmp = cpu.cop0.reg.read(inst.copz_type1.rd);
        // FIXME: T+1 (delay)
        cpu.gpr.write(inst.copz_type1.rt, tmp);
    }

    static void op_dmtc0(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L235
        Utils::trace("DMTC0: COP0.reg[{}] <= {}",
                     static_cast<uint32_t>(inst.copz_type1.rd),
                     GPR_NAMES[inst.copz_type1.rt]);
        const uint64_t tmp = cpu.gpr.read(inst.copz_type1.rt);
        // FIXME: T+1 (delay)
        cpu.cop0.reg.write(inst.copz_type1.rd, tmp);
    }

    // TODO: move to another file
    static void op_cfc1(Cpu &cpu, instruction_t inst) {
        Utils::abort("CFC1: not CpuImplemented");
    }
};

} // namespace Cpu
} // namespace N64

#endif

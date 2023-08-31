#ifndef CPU_INSTRUCTION_IMPL_H
#define CPU_INSTRUCTION_IMPL_H

#include "cop0.h"
#include "cpu.h"
#include "instruction.h"
#include "memory/bus.h"
#include "mmu/mmu.h"
#include "mmu/tlb.h"
#include "utils/utils.h"
#include <cstdint>
#include <optional>

namespace N64 {
namespace Cpu {

class Cpu::CpuImpl {
  public:
    static void op_add(Cpu &cpu, instruction_t inst) {
        // TODO: throw exception
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L903
        assert_encoding_is_valid(inst.r_type.sa == 0);
        uint32_t rs = cpu.gpr.read(inst.r_type.rs);
        uint32_t rt = cpu.gpr.read(inst.r_type.rt);
        uint32_t res = rs + rt;
        instruction_trace("ADD: {} <= {} + {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, (int64_t)((int32_t)res));
    }

    static void op_addu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L915
        assert_encoding_is_valid(inst.r_type.sa == 0);
        uint32_t rs = cpu.gpr.read(inst.r_type.rs);
        uint32_t rt = cpu.gpr.read(inst.r_type.rt);
        int32_t res = rs + rt;
        instruction_trace("ADDU: {} <= {} + {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_dadd(Cpu &cpu, instruction_t inst) {
        // TODO: exception
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L980
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        uint64_t rt = cpu.gpr.read(inst.r_type.rt);
        uint64_t res = rs + rt;
        instruction_trace("DADD: {} <= {} + {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_daddu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L992
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        uint64_t rt = cpu.gpr.read(inst.r_type.rt);
        uint64_t res = rs + rt;
        instruction_trace("DADDU: {} <= {} + {}", GPR_NAMES[inst.r_type.rd],
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
        instruction_trace("SUB: {} <= {} - {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_subu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L946
        assert_encoding_is_valid(inst.r_type.sa == 0);
        uint32_t rs = cpu.gpr.read(inst.r_type.rs);
        uint32_t rt = cpu.gpr.read(inst.r_type.rt);
        int32_t res = rs - rt;
        instruction_trace("SUBU: {} <= {} - {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_dsub(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L999
        // TODO: check overflow
        int64_t minuend = cpu.gpr.read(inst.r_type.rs);
        int64_t subtrahend = cpu.gpr.read(inst.r_type.rt);
        int64_t diff = minuend - subtrahend;
        cpu.gpr.write(inst.r_type.rd, diff);
        instruction_trace("DSUB: {} <= {} - {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
    }

    static void op_dsubu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1011
        uint64_t minuend = cpu.gpr.read(inst.r_type.rs);
        uint64_t subtrahend = cpu.gpr.read(inst.r_type.rt);
        uint64_t diff = minuend - subtrahend;
        cpu.gpr.write(inst.r_type.rd, diff);
        instruction_trace("DSUBU: {} <= {} - {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
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
        instruction_trace("MULT {}, {}", GPR_NAMES[inst.r_type.rs],
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
        instruction_trace("MULTU {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        cpu.lo = sext_lo;
        cpu.hi = sext_hi;
    }

    static void op_dmult(Cpu &cpu, instruction_t inst) {
        instruction_trace("DMULT {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        uint64_t rt = cpu.gpr.read(inst.r_type.rt);
        uint64_t lower = rs * rt;
        uint64_t upper = mul_signed_hi(rs, rt);
        cpu.lo = lower;
        cpu.hi = upper;
    }

    static void op_dmultu(Cpu &cpu, instruction_t inst) {
        instruction_trace("DMULTU {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        int64_t rs = cpu.gpr.read(inst.r_type.rs);
        int64_t rt = cpu.gpr.read(inst.r_type.rt);
        int64_t lower = rs * rt;
        uint64_t upper = mul_unsigned_hi(rs, rt);
        cpu.lo = static_cast<uint64_t>(lower);
        cpu.hi = upper;
    }

    static void op_div(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L809
        int64_t dividend = (int32_t)cpu.gpr.read(inst.r_type.rs);
        int64_t divisor = (int32_t)cpu.gpr.read(inst.r_type.rt);
        instruction_trace("DIV {}, {}", GPR_NAMES[inst.r_type.rs],
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
        instruction_trace("DIVU {}, {}", GPR_NAMES[inst.r_type.rs],
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

    static void op_ddiv(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L863
        instruction_trace("DDIVU {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        int64_t dividend = cpu.gpr.read(inst.r_type.rs);
        int64_t divisor = cpu.gpr.read(inst.r_type.rt);

        if (divisor == 0) {
            Utils::unimplemented("DDIV division by zero");
        } else if (divisor == -1 && dividend == INT64_MIN) {
            Utils::unimplemented("DDIV overflow");
            // cpu.lo = dividend;
            // cpu.hi = 0;
        } else {
            int64_t quotient = (int64_t)(dividend / divisor);
            int64_t remainder = (int64_t)(dividend % divisor);
            cpu.lo = quotient;
            cpu.hi = remainder;
        }
    }

    static void op_ddivu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L887
        instruction_trace("DDIVU {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        uint64_t dividend = cpu.gpr.read(inst.r_type.rs);
        uint64_t divisor = cpu.gpr.read(inst.r_type.rt);

        if (divisor == 0) {
            cpu.lo = 0xFFFFFFFF'FFFFFFFF;
            cpu.hi = dividend;
        } else {
            uint64_t quotient = dividend / divisor;
            uint64_t remainder = dividend % divisor;
            cpu.lo = quotient;
            cpu.hi = remainder;
        }
    }

    static void op_sll(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L698
        assert_encoding_is_valid(inst.r_type.rs == 0);
        int32_t res = cpu.gpr.read(inst.r_type.rt) << inst.r_type.sa;
        if (inst.raw == 0) {
            instruction_trace("NOP");
        } else {
            instruction_trace("SLL: {} <= {} << {}", GPR_NAMES[inst.r_type.rd],
                              GPR_NAMES[inst.r_type.rt],
                              (uint8_t)inst.r_type.sa);
        }
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_srl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L703
        uint32_t value = cpu.gpr.read(inst.r_type.rt);
        int32_t res = value >> inst.r_type.sa;
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
        instruction_trace("SRL {} {} {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_sra(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L709
        int64_t value = cpu.gpr.read(inst.r_type.rt);
        int32_t res = (int64_t)(value >> (uint64_t)inst.r_type.sa);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
        instruction_trace("SRA {} {} {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_srav(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L715
        int64_t value = cpu.gpr.read(inst.r_type.rt);
        int32_t res =
            (int64_t)(value >> (cpu.gpr.read(inst.r_type.rs) & 0b11111));
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
        instruction_trace("SRAV {}, {}, {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], GPR_NAMES[inst.r_type.rs]);
    }

    static void op_sllv(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L721
        assert_encoding_is_valid(inst.r_type.sa == 0);
        instruction_trace("SLLV {}, {}, {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], GPR_NAMES[inst.r_type.rs]);
        uint32_t value = cpu.gpr.read(inst.r_type.rt); // as 32bit
        int32_t res = value << (cpu.gpr.read(inst.r_type.rs) & 0b11111);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_srlv(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L727
        assert_encoding_is_valid(inst.r_type.sa == 0);
        instruction_trace("SRLV {}, {}, {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], GPR_NAMES[inst.r_type.rs]);
        uint32_t value = cpu.gpr.read(inst.r_type.rt); // as 32bit
        int32_t res = value >> (cpu.gpr.read(inst.r_type.rs) & 0b11111);
        cpu.gpr.write(inst.r_type.rd, (int64_t)res);
    }

    static void op_slt(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L962
        int64_t rs = cpu.gpr.read(inst.r_type.rs);
        int64_t rt = cpu.gpr.read(inst.r_type.rt);
        instruction_trace("SLT {} {} {} ", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, rs < rt ? 1 : 0);
    }

    static void op_sltu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L968
        assert_encoding_is_valid(inst.r_type.sa == 0);
        uint64_t rs = cpu.gpr.read(inst.r_type.rs); // unsigned
        uint64_t rt = cpu.gpr.read(inst.r_type.rt); // unsigned
        instruction_trace("SLTU {} {} {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, rs < rt ? 1 : 0);
    }

    static void op_slti(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L199
        int16_t imm = inst.i_type.imm;
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        instruction_trace("SLTI {} {} {}", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, rs < imm ? 1 : 0);
    }

    static void op_sltiu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L210
        int16_t imm = inst.i_type.imm;
        uint64_t rs = cpu.gpr.read(inst.i_type.rs);
        instruction_trace("SLTIU {} {} {}", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, rs < imm ? 1 : 0);
    }

    static void op_and(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L922
        assert_encoding_is_valid(inst.r_type.sa == 0);
        instruction_trace("AND: {} <= {} & {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, cpu.gpr.read(inst.r_type.rs) &
                                          cpu.gpr.read(inst.r_type.rt));
    }

    static void op_or(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L954
        assert_encoding_is_valid(inst.r_type.sa == 0);
        instruction_trace("OR: {} <= {} | {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, cpu.gpr.read(inst.r_type.rs) |
                                          cpu.gpr.read(inst.r_type.rt));
    }

    static void op_xor(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L958
        assert_encoding_is_valid(inst.r_type.sa == 0);
        instruction_trace("XOR: {} <= {} ^ {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        cpu.gpr.write(inst.r_type.rd, cpu.gpr.read(inst.r_type.rs) ^
                                          cpu.gpr.read(inst.r_type.rt));
    }

    static void op_nor(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L958
        instruction_trace("NOR: {}, {}, {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
        uint64_t res =
            ~(cpu.gpr.read(inst.r_type.rs) | cpu.gpr.read(inst.r_type.rt));
        cpu.gpr.write(inst.r_type.rd, res);
    }

    static void op_jr(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L733
        assert_encoding_is_valid(inst.r_type.rt == 0 && inst.r_type.rd == 0 &&
                                 inst.r_type.sa == 0);
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        instruction_trace("JR {}", GPR_NAMES[inst.r_type.rs]);
        branch_addr64(cpu, true, rs);
    }

    static void op_jalr(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L737
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        branch_addr64(cpu, true, rs);
        link(cpu, inst.r_type.rd);
        instruction_trace("JALR addr={} dest={}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rd]);
    }

    static void op_mfhi(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L746
        assert_encoding_is_valid(inst.r_type.rs == 0 && inst.r_type.rt == 0 &&
                                 inst.r_type.sa == 0);
        instruction_trace("MFHI {} <= hi", GPR_NAMES[inst.r_type.rd]);
        cpu.gpr.write(inst.r_type.rd, cpu.hi);
    }

    static void op_mflo(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L754
        assert_encoding_is_valid(inst.r_type.rs == 0 && inst.r_type.rt == 0 &&
                                 inst.r_type.sa == 0);
        instruction_trace("MFLO {} <= lo", GPR_NAMES[inst.r_type.rd]);
        cpu.gpr.write(inst.r_type.rd, cpu.lo);
    }

    static void op_mthi(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L750
        instruction_trace("MTHI hi <= {}", GPR_NAMES[inst.r_type.rs]);
        cpu.hi = cpu.gpr.read(inst.r_type.rs);
    }

    static void op_mtlo(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L758
        instruction_trace("MTLO lo <= {}", GPR_NAMES[inst.r_type.rs]);
        cpu.lo = cpu.gpr.read(inst.r_type.rs);
    }

    static void op_eret(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1151
        instruction_trace("ERET");
        if (cpu.cop0.reg.status.erl) {
            cpu.set_pc64(cpu.cop0.reg.error_epc);
            cpu.cop0.reg.status.erl = false;
        } else {
            cpu.set_pc64(cpu.cop0.reg.epc);
            cpu.cop0.reg.status.exl = false;
        }
        cpu.cop0.llbit = false;
    }

    static void op_tlbwi(Cpu &cpu, instruction_t inst) {
        g_tlb().write_entry(false);
    }

    static void op_tge(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1040
        int64_t rs = cpu.gpr.read(inst.r_type.rs);
        int64_t rt = cpu.gpr.read(inst.r_type.rt);
        instruction_trace("TGE: {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        if (rs >= rt)
            cpu.handle_exception(ExceptionCode::TRAP, 0, true);
    }

    static void op_tgeu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1049
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        uint64_t rt = cpu.gpr.read(inst.r_type.rt);
        instruction_trace("TGEU: {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        if (rs >= rt)
            cpu.handle_exception(ExceptionCode::TRAP, 0, true);
    }

    static void op_tlt(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1058
        int64_t rs = cpu.gpr.read(inst.r_type.rs);
        int64_t rt = cpu.gpr.read(inst.r_type.rt);
        instruction_trace("TLT: {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        if (rs < rt)
            cpu.handle_exception(ExceptionCode::TRAP, 0, true);
    }

    static void op_tltu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1067
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        uint64_t rt = cpu.gpr.read(inst.r_type.rt);
        instruction_trace("TLTU: {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        if (rs < rt)
            cpu.handle_exception(ExceptionCode::TRAP, 0, true);
    }

    static void op_teq(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1018
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        uint64_t rt = cpu.gpr.read(inst.r_type.rt);
        instruction_trace("TEQ: {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        if (rs == rt)
            cpu.handle_exception(ExceptionCode::TRAP, 0, true);
    }

    static void op_tne(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1031
        uint64_t rs = cpu.gpr.read(inst.r_type.rs);
        uint64_t rt = cpu.gpr.read(inst.r_type.rt);
        instruction_trace("TNE: {}, {}", GPR_NAMES[inst.r_type.rs],
                          GPR_NAMES[inst.r_type.rt]);
        if (rs != rt)
            cpu.handle_exception(ExceptionCode::TRAP, 0, true);
    }

    static void op_dsll(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1077
        uint64_t value = cpu.gpr.read(inst.r_type.rt);
        value <<= inst.r_type.sa;
        cpu.gpr.write(inst.r_type.rd, value);
        instruction_trace("DSLL {} <= {} << {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_dsrl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1083
        uint64_t value = cpu.gpr.read(inst.r_type.rt);
        value >>= inst.r_type.sa;
        cpu.gpr.write(inst.r_type.rd, value);
        instruction_trace("DSRL {} <= {} >> {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_dsra(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1089
        int64_t value = cpu.gpr.read(inst.r_type.rt);
        value >>= inst.r_type.sa;
        cpu.gpr.write(inst.r_type.rd, value);
        instruction_trace("DSRA {} <= {} >> {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_dsll32(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1095
        uint64_t value = cpu.gpr.read(inst.r_type.rt);
        value <<= (inst.r_type.sa + 32);
        cpu.gpr.write(inst.r_type.rd, value);
        instruction_trace("DSLL32 {} <= {} << {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_dsrl32(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1101
        uint64_t value = cpu.gpr.read(inst.r_type.rt);
        value >>= (inst.r_type.sa + 32);
        cpu.gpr.write(inst.r_type.rd, value);
        instruction_trace("DSRL32 {} <= {} >> {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_dsra32(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1107
        int64_t value = cpu.gpr.read(inst.r_type.rt);
        value >>= (inst.r_type.sa + 32);
        cpu.gpr.write(inst.r_type.rd, value);
        instruction_trace("DSRA32 {} <= {} >> {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], (uint8_t)inst.r_type.sa);
    }

    static void op_sync(Cpu &cpu, instruction_t inst) {
        instruction_trace("SYNC");
    }

    static void op_bltz(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1113
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        instruction_trace("BLTZ {}", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs < 0, inst);
    }

    static void op_bltzl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1118
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        instruction_trace("BLTZL {}", GPR_NAMES[inst.i_type.rs]);
        branch_likely_offset16(cpu, rs < 0, inst);
    }

    static void op_bgez(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1123
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        instruction_trace("BGEZ {}", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs >= 0, inst);
    }

    static void op_bgezl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1128
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        instruction_trace("BGEZL {}", GPR_NAMES[inst.i_type.rs]);
        branch_likely_offset16(cpu, rs >= 0, inst);
    }

    static void op_bltzal(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1133
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        instruction_trace("BLTZAL cond: {} < 0", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs < 0, inst);
        link(cpu, RA);
    }

    static void op_bgezal(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L1139
        int64_t rs = cpu.gpr.read(inst.i_type.rs);
        instruction_trace("BGEZAL cond: {} >= 0", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs >= 0, inst);
        link(cpu, RA);
    }

    static void op_j(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L181
        uint64_t target = inst.j_type.target;
        target <<= 2;
        // FIXME: should use prev_pc?
        // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/interpreter/instructions.cpp#L607
        target |= ((cpu.pc - 4) & 0xFFFFFFFF'F0000000); // pc is now 4 ahead
        instruction_trace("J {:#x}", target);
        branch_addr64(cpu, true, target);
    }

    static void op_jal(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L189
        link(cpu, RA);
        uint64_t target = inst.j_type.target;
        target <<= 2;
        target |= ((cpu.pc - 4) & 0xFFFFFFFF'F0000000); // pc is now 4 ahead
        instruction_trace("JAL {:#x}", target);
        branch_addr64(cpu, true, target);
    }

    static void op_lb(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L437
        int16_t offset = inst.i_type.imm;
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        instruction_trace("LB {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], offset);
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            int8_t value = Memory::read_paddr8(paddr.value());
            cpu.gpr.write(inst.i_type.rt, (int64_t)value);
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_lbu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L268
        int16_t offset = inst.i_type.imm;
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        instruction_trace("LBU {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], offset);
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint8_t value = Memory::read_paddr8(paddr.value());
            cpu.gpr.write(inst.i_type.rt, value);
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_lh(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L300
        // TODO: alignment?
        int16_t offset = inst.i_type.imm;
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        instruction_trace("LH {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], offset);
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            int16_t value = Memory::read_paddr16(paddr.value());
            cpu.gpr.write(inst.i_type.rt, (int64_t)value);
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_lhu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L282
        // TODO: alignment?
        int16_t offset = inst.i_type.imm;
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        instruction_trace("LHU: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], offset);
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint16_t value = Memory::read_paddr16(paddr.value());
            cpu.gpr.write(inst.i_type.rt, static_cast<uint64_t>(value)); // zext
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_lw(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L317
        int16_t offset = inst.i_type.imm;
        instruction_trace("LW: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], offset);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            int32_t word = Memory::read_paddr32(paddr.value());
            cpu.gpr.write(inst.i_type.rt, (int64_t)word); // sext
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_lwu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L336
        int16_t offset = inst.i_type.imm;
        instruction_trace("LWU: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], offset);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint32_t word = Memory::read_paddr32(paddr.value());
            cpu.gpr.write(inst.i_type.rt, (uint64_t)word); // zext
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_lui(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L259
        assert_encoding_is_valid(inst.i_type.rs == 0);
        int64_t simm = (int16_t)inst.i_type.imm; // sext
        // 負数の左シフトはUBなので乗算で実装
        simm *= 65536;
        instruction_trace("LUI: {} <= {:#x}", GPR_NAMES[inst.i_type.rt], simm);
        cpu.gpr.write(inst.i_type.rt, simm);
    }

    static void op_ld(Cpu &cpu, instruction_t inst) {
        // https://hack64.net/docs/VR43XX.pdf p.441
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L282
        int16_t offset = inst.i_type.imm;
        instruction_trace("LD: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], offset);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint64_t value = Memory::read_paddr64(paddr.value());
            cpu.gpr.write(inst.i_type.rt, value);
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_ldl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L519
        int16_t offset = inst.fi_type.offset;
        uint64_t vaddr = cpu.gpr.read(inst.fi_type.base) + offset;
        // TODO: trace log
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);
        if (paddr.has_value()) {
            int32_t shift = 8 * ((vaddr ^ 0) & 7);
            uint64_t mask = (uint64_t)0xFFFFFFFFFFFFFFFF << shift;
            uint64_t data = Memory::read_paddr64(paddr.value() & ~7);
            uint64_t old = cpu.gpr.read(inst.i_type.rt);
            cpu.gpr.write(inst.i_type.rt, (old & ~mask) | (data << shift));
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_ldr(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L536
        int16_t offset = inst.fi_type.offset;
        uint64_t vaddr = cpu.gpr.read(inst.fi_type.base) + offset;
        // TODO: trace log
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);
        if (paddr.has_value()) {
            int32_t shift = 8 * ((vaddr ^ 7) & 7);
            uint64_t mask = (uint64_t)0xFFFFFFFFFFFFFFFF >> shift;
            uint64_t data = Memory::read_paddr64(paddr.value() & ~7);
            uint64_t old = cpu.gpr.read(inst.i_type.rt);
            cpu.gpr.write(inst.i_type.rt, (old & ~mask) | (data >> shift));
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_ll(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L588
        int16_t offset = inst.i_type.imm;
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        instruction_trace("LL: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], offset);
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            int32_t word = Memory::read_paddr32(paddr.value());
            cpu.gpr.write(inst.i_type.rt, (int64_t)word); // sext

            cpu.cop0.reg.lladdr = paddr.value() >> 4;
            cpu.cop0.llbit = 1;
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_lld(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L615
        // TODO: exception when 64bit?
        int16_t offset = inst.i_type.imm;
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        instruction_trace("LLD: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], offset);
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint64_t value = Memory::read_paddr64(paddr.value());
            cpu.gpr.write(inst.i_type.rt, value);

            cpu.cop0.reg.lladdr = paddr.value() >> 4;
            cpu.cop0.llbit = 1;
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::LOAD), 0,
                                 true);
        }
    }

    static void op_sb(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L353
        int16_t offset = inst.i_type.imm;
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        instruction_trace("SB: *({} + {:#x}) <= {}", GPR_NAMES[inst.i_type.rs],
                          offset, GPR_NAMES[inst.r_type.rt]);
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint8_t value = cpu.gpr.read(inst.r_type.rt);
            Memory::write_paddr8(paddr.value(), value);
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::STORE), 0,
                                 true);
        }
    }

    static void op_sh(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L368
        int16_t offset = inst.i_type.imm;
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        instruction_trace("SH: *({} + {:#x}) <= {}", GPR_NAMES[inst.i_type.rs],
                          offset, GPR_NAMES[inst.r_type.rt]);
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint16_t value = cpu.gpr.read(inst.r_type.rt);
            Memory::write_paddr8(paddr.value(), value);
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::STORE), 0,
                                 true);
        }
    }

    static void op_sw(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L382
        int16_t offset = inst.i_type.imm;
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        instruction_trace("SW: *({} + {:#x}) <= {}", GPR_NAMES[inst.i_type.rs],
                          offset, GPR_NAMES[inst.r_type.rt]);
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint32_t word = cpu.gpr.read(inst.r_type.rt);
            Memory::write_paddr32(paddr.value(), word);
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::STORE), 0,
                                 true);
        }
    }

    static void op_sd(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L402
        int16_t offset = inst.i_type.imm;
        instruction_trace("SD: *({} + {:#x}) <= {}", GPR_NAMES[inst.i_type.rs],
                          offset, GPR_NAMES[inst.r_type.rt]);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);

        if (paddr.has_value()) {
            uint64_t dword = cpu.gpr.read(inst.r_type.rt);
            Memory::write_paddr64(paddr.value(), dword);
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::STORE), 0,
                                 true);
        }
    }

    static void op_sdl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L553
        int16_t offset = inst.fi_type.offset;
        uint64_t vaddr = cpu.gpr.read(inst.fi_type.base) + offset;
        // TODO: trace log
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);
        if (paddr.has_value()) {
            int32_t shift = 8 * ((vaddr ^ 0) & 7);
            uint64_t mask = 0xFFFFFFFFFFFFFFFF >> shift;
            uint64_t data = Memory::read_paddr64(paddr.value() & ~7);
            uint64_t old = cpu.gpr.read(inst.i_type.rt);
            Memory::write_paddr64(paddr.value() & ~7,
                                  (data & ~mask) | (old >> shift));
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::STORE), 0,
                                 true);
        }
    }

    static void op_sdr(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L571
        int16_t offset = inst.fi_type.offset;
        uint64_t vaddr = cpu.gpr.read(inst.fi_type.base) + offset;
        // TODO: trace log
        std::optional<uint32_t> paddr = Mmu::resolve_vaddr(vaddr);
        if (paddr.has_value()) {
            int32_t shift = 8 * ((vaddr ^ 7) & 7);
            uint64_t mask = (uint64_t)0xFFFFFFFFFFFFFFFF << shift;
            uint64_t data = Memory::read_paddr64(paddr.value() & ~7);
            uint64_t old = cpu.gpr.read(inst.i_type.rt);
            Memory::write_paddr64(paddr.value() & ~7,
                                  (data & ~mask) | (old << shift));
        } else {
            cpu.handle_exception(get_tlb_exception_code(BusAccess::STORE), 0,
                                 true);
        }
    }

    static void op_sc(Cpu &cpu, instruction_t inst) {
        Utils::unimplemented("SC");
    }

    static void op_scd(Cpu &cpu, instruction_t inst) {
        Utils::unimplemented("SCD");
    }

    static void op_addi(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L99
        uint32_t rs = cpu.gpr.read(inst.i_type.rs);
        uint32_t addend = (int32_t)((int16_t)inst.i_type.imm); // sext
        uint32_t res = rs + addend;
        instruction_trace("ADDI: {} <= {} + {:#x}", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], addend);
        // TODO: check overflow
        cpu.gpr.write(inst.i_type.rt, (int64_t)((int32_t)res));
    }

    static void op_addiu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L110
        uint32_t rs = cpu.gpr.read(inst.i_type.rs);
        int16_t addend = inst.i_type.imm; // sext
        int32_t res = rs + addend;
        instruction_trace("ADDIU: {} <= {} + {:#x}", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], addend);
        cpu.gpr.write(inst.i_type.rt, (int64_t)res);
    }

    static void op_daddi(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L110
        uint64_t addend = (int64_t)((int16_t)inst.i_type.imm);
        uint64_t rs = cpu.gpr.read(inst.i_type.rs);
        uint64_t res = rs + addend;
        // TODO: check overflow
        instruction_trace("DADDI: {} <= {} + {:#x}", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], addend);
        cpu.gpr.write(inst.i_type.rt, res);
    }

    static void op_daddiu(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L430
        uint64_t addend = (int64_t)((int16_t)inst.i_type.imm);
        uint64_t rs = cpu.gpr.read(inst.i_type.rs);
        uint64_t res = rs + addend;
        instruction_trace("DADDIU: {} <= {} + {:#x}", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], addend);
        cpu.gpr.write(inst.i_type.rt, res);
    }

    static void op_andi(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L131
        uint64_t imm = inst.i_type.imm; // zext
        instruction_trace("ANDI: {} <= {} & {:#x}", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, cpu.gpr.read(inst.i_type.rs) & imm);
    }

    static void op_ori(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L422
        uint64_t imm = inst.i_type.imm; // zext
        instruction_trace("ORI: {} <= {} | {:#x}", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, cpu.gpr.read(inst.i_type.rs) | imm);
    }

    static void op_xori(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L426
        uint64_t imm = inst.i_type.imm; // zext
        instruction_trace("XORI: {} <= {} ^ {:#x}", GPR_NAMES[inst.i_type.rt],
                          GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, cpu.gpr.read(inst.i_type.rs) ^ imm);
    }

    static void op_beq(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L137
        instruction_trace("BEQ: cond {} == {}", GPR_NAMES[inst.i_type.rs],
                          GPR_NAMES[inst.i_type.rt]);
        branch_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) == cpu.gpr.read(inst.i_type.rt),
            inst);
    }

    static void op_beql(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L141
        instruction_trace("BEQL: cond {} == {}", GPR_NAMES[inst.i_type.rs],
                          GPR_NAMES[inst.i_type.rt]);
        branch_likely_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) == cpu.gpr.read(inst.i_type.rt),
            inst);
    }

    static void op_bne(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L165
        instruction_trace("BNE: cond {} != {}", GPR_NAMES[inst.i_type.rs],
                          GPR_NAMES[inst.i_type.rt]);
        instruction_trace("{} : {:#x}, {} = {:#x}", GPR_NAMES[inst.i_type.rs],
                          cpu.gpr.read(inst.i_type.rs),
                          GPR_NAMES[inst.i_type.rt],
                          cpu.gpr.read(inst.i_type.rt));
        branch_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) != cpu.gpr.read(inst.i_type.rt),
            inst);
    }

    static void op_bnel(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L169
        instruction_trace("BNEL: cond {} != {}", GPR_NAMES[inst.i_type.rs],
                          GPR_NAMES[inst.i_type.rt]);
        branch_likely_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) != cpu.gpr.read(inst.i_type.rt),
            inst);
    }

    static void op_blez(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L155
        assert_encoding_is_valid(inst.i_type.rt == 0);
        int64_t rs = cpu.gpr.read(inst.i_type.rs); // as signed integer
        instruction_trace("BLEZ: cond {} <= 0", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs <= 0, inst);
    }

    static void op_blezl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L160
        assert_encoding_is_valid(inst.i_type.rt == 0);
        int64_t rs = cpu.gpr.read(inst.i_type.rs); // as signed integer
        instruction_trace("BLEZL: cond {} <= 0", GPR_NAMES[inst.i_type.rs]);
        branch_likely_offset16(cpu, rs <= 0, inst);
    }

    static void op_bgtz(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L145
        assert_encoding_is_valid(inst.i_type.rt == 0);
        int64_t rs = cpu.gpr.read(inst.i_type.rs); // as signed integer
        instruction_trace("BGTZ: cond {} > 0", GPR_NAMES[inst.i_type.rs]);
        branch_offset16(cpu, rs > 0, inst);
    }

    static void op_bgtzl(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L150
        assert_encoding_is_valid(inst.i_type.rt == 0);
        int64_t rs = cpu.gpr.read(inst.i_type.rs); // as signed integer
        instruction_trace("BGTZL: cond {} > 0", GPR_NAMES[inst.i_type.rs]);
        branch_likely_offset16(cpu, rs > 0, inst);
    }

    static void op_cache() {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L177
        // B.1.1 CACHE Instruction
        // https://hack64.net/docs/VR43XX.pdf
        // no need for emulation?
        instruction_trace("CACHE: no effect");
    }

    static void op_mfc0(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L220
        instruction_trace("MFC0: {} <= COP0.reg[{}]",
                          GPR_NAMES[inst.cop_r_like.rt],
                          static_cast<uint32_t>(inst.cop_r_like.rd));
        int32_t tmp = cpu.cop0.reg.read(inst.cop_r_like.rd);
        int64_t stmp = tmp; // sext
        // FIXME: T+1 (delay)
        cpu.gpr.write(inst.cop_r_like.rt, stmp);
    }

    static void op_mtc0(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L225
        instruction_trace("MTC0: COP0.reg[{}] <= {}",
                          static_cast<uint32_t>(inst.cop_r_like.rd),
                          GPR_NAMES[inst.cop_r_like.rt]);
        uint32_t tmp = cpu.gpr.read(inst.cop_r_like.rt);
        // FIXME: T+1 (delay)
        cpu.cop0.reg.write(inst.cop_r_like.rd, tmp);
    }

    static void op_dmfc0(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L230
        instruction_trace("DMFC0: {} <= COP0.reg[{}]",
                          GPR_NAMES[inst.cop_r_like.rt],
                          static_cast<uint32_t>(inst.cop_r_like.rd));
        uint64_t tmp = cpu.cop0.reg.read(inst.cop_r_like.rd);
        // FIXME: T+1 (delay)
        cpu.gpr.write(inst.cop_r_like.rt, tmp);
    }

    static void op_dmtc0(Cpu &cpu, instruction_t inst) {
        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L235
        instruction_trace("DMTC0: COP0.reg[{}] <= {}",
                          static_cast<uint32_t>(inst.cop_r_like.rd),
                          GPR_NAMES[inst.cop_r_like.rt]);
        const uint64_t tmp = cpu.gpr.read(inst.cop_r_like.rt);
        // FIXME: T+1 (delay)
        cpu.cop0.reg.write(inst.cop_r_like.rd, tmp);
    }
};

} // namespace Cpu
} // namespace N64

#endif

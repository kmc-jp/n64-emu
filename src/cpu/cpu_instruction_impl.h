#ifndef CPU_INSTRUCTION_IMPL_H
#define CPU_INSTRUCTION_IMPL_H

#include "cpu/instruction.h"

namespace N64::Cpu {
class Cpu;
}

namespace N64 {
namespace Cpu {

class CpuImpl {
  public:
    static void op_add(Cpu &cpu, instruction_t inst);

    static void op_addu(Cpu &cpu, instruction_t inst);

    static void op_dadd(Cpu &cpu, instruction_t inst);

    static void op_daddu(Cpu &cpu, instruction_t inst);

    static void op_sub(Cpu &cpu, instruction_t inst);

    static void op_subu(Cpu &cpu, instruction_t inst);

    static void op_dsub(Cpu &cpu, instruction_t inst);

    static void op_dsubu(Cpu &cpu, instruction_t inst);

    static void op_mult(Cpu &cpu, instruction_t inst);

    static void op_multu(Cpu &cpu, instruction_t inst);

    static void op_dmult(Cpu &cpu, instruction_t inst);

    static void op_dmultu(Cpu &cpu, instruction_t inst);

    static void op_div(Cpu &cpu, instruction_t inst);

    static void op_divu(Cpu &cpu, instruction_t inst);

    static void op_ddiv(Cpu &cpu, instruction_t inst);

    static void op_ddivu(Cpu &cpu, instruction_t inst);

    static void op_sll(Cpu &cpu, instruction_t inst);

    static void op_srl(Cpu &cpu, instruction_t inst);

    static void op_sra(Cpu &cpu, instruction_t inst);

    static void op_srav(Cpu &cpu, instruction_t inst);

    static void op_sllv(Cpu &cpu, instruction_t inst);

    static void op_srlv(Cpu &cpu, instruction_t inst);

    static void op_slt(Cpu &cpu, instruction_t inst);

    static void op_sltu(Cpu &cpu, instruction_t inst);

    static void op_slti(Cpu &cpu, instruction_t inst);

    static void op_sltiu(Cpu &cpu, instruction_t inst);

    static void op_and(Cpu &cpu, instruction_t inst);

    static void op_or(Cpu &cpu, instruction_t inst);

    static void op_xor(Cpu &cpu, instruction_t inst);

    static void op_nor(Cpu &cpu, instruction_t inst);

    static void op_jr(Cpu &cpu, instruction_t inst);

    static void op_jalr(Cpu &cpu, instruction_t inst);

    static void op_mfhi(Cpu &cpu, instruction_t inst);

    static void op_mflo(Cpu &cpu, instruction_t inst);

    static void op_mthi(Cpu &cpu, instruction_t inst);

    static void op_mtlo(Cpu &cpu, instruction_t inst);

    static void op_eret(Cpu &cpu, instruction_t inst);

    static void op_tlbwi(Cpu &cpu, instruction_t inst);

    static void op_tge(Cpu &cpu, instruction_t inst);

    static void op_tgeu(Cpu &cpu, instruction_t inst);

    static void op_tlt(Cpu &cpu, instruction_t inst);

    static void op_tltu(Cpu &cpu, instruction_t inst);

    static void op_teq(Cpu &cpu, instruction_t inst);

    static void op_tne(Cpu &cpu, instruction_t inst);

    static void op_dsll(Cpu &cpu, instruction_t inst);

    static void op_dsrl(Cpu &cpu, instruction_t inst);

    static void op_dsra(Cpu &cpu, instruction_t inst);

    static void op_dsll32(Cpu &cpu, instruction_t inst);

    static void op_dsrl32(Cpu &cpu, instruction_t inst);

    static void op_dsra32(Cpu &cpu, instruction_t inst);

    static void op_sync(Cpu &cpu, instruction_t inst);

    static void op_bltz(Cpu &cpu, instruction_t inst);

    static void op_bltzl(Cpu &cpu, instruction_t inst);

    static void op_bgez(Cpu &cpu, instruction_t inst);

    static void op_bgezl(Cpu &cpu, instruction_t inst);

    static void op_bltzal(Cpu &cpu, instruction_t inst);

    static void op_bgezal(Cpu &cpu, instruction_t inst);

    static void op_j(Cpu &cpu, instruction_t inst);

    static void op_jal(Cpu &cpu, instruction_t inst);

    static void op_lb(Cpu &cpu, instruction_t inst);

    static void op_lbu(Cpu &cpu, instruction_t inst);

    static void op_lh(Cpu &cpu, instruction_t inst);

    static void op_lhu(Cpu &cpu, instruction_t inst);

    static void op_lw(Cpu &cpu, instruction_t inst);

    static void op_lwu(Cpu &cpu, instruction_t inst);

    static void op_lui(Cpu &cpu, instruction_t inst);

    static void op_ld(Cpu &cpu, instruction_t inst);

    static void op_ldl(Cpu &cpu, instruction_t inst);

    static void op_ldr(Cpu &cpu, instruction_t inst);

    static void op_ll(Cpu &cpu, instruction_t inst);

    static void op_lld(Cpu &cpu, instruction_t inst);

    static void op_sb(Cpu &cpu, instruction_t inst);

    static void op_sh(Cpu &cpu, instruction_t inst);

    static void op_sw(Cpu &cpu, instruction_t inst);

    static void op_sd(Cpu &cpu, instruction_t inst);

    static void op_sdl(Cpu &cpu, instruction_t inst);

    static void op_sdr(Cpu &cpu, instruction_t inst);

    static void op_sc(Cpu &cpu, instruction_t inst);

    static void op_scd(Cpu &cpu, instruction_t inst);

    static void op_addi(Cpu &cpu, instruction_t inst);

    static void op_addiu(Cpu &cpu, instruction_t inst);

    static void op_daddi(Cpu &cpu, instruction_t inst);

    static void op_daddiu(Cpu &cpu, instruction_t inst);

    static void op_andi(Cpu &cpu, instruction_t inst);

    static void op_ori(Cpu &cpu, instruction_t inst);

    static void op_xori(Cpu &cpu, instruction_t inst);

    static void op_beq(Cpu &cpu, instruction_t inst);

    static void op_beql(Cpu &cpu, instruction_t inst);

    static void op_bne(Cpu &cpu, instruction_t inst);

    static void op_bnel(Cpu &cpu, instruction_t inst);

    static void op_blez(Cpu &cpu, instruction_t inst);

    static void op_blezl(Cpu &cpu, instruction_t inst);

    static void op_bgtz(Cpu &cpu, instruction_t inst);

    static void op_bgtzl(Cpu &cpu, instruction_t inst);

    static void op_cache();

    static void op_mfc0(Cpu &cpu, instruction_t inst);

    static void op_mtc0(Cpu &cpu, instruction_t inst);

    static void op_dmfc0(Cpu &cpu, instruction_t inst);

    static void op_dmtc0(Cpu &cpu, instruction_t inst);
};

} // namespace Cpu
} // namespace N64

#endif

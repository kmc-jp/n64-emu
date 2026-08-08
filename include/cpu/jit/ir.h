#ifndef CPU_JIT_IR_H
#define CPU_JIT_IR_H

#include <cstdint>
#include <vector>

namespace N64 {
namespace Cpu {
namespace Jit {

constexpr int MAX_BLOCK_INSNS = 64;

enum class IrOpKind : uint8_t {
    Nop,
    // R-type ALU (32-bit result, sign-extended to 64)
    Add,
    Addu,
    Sub,
    Subu,
    And,
    Or,
    Xor,
    Nor,
    Slt,
    Sltu,
    // 64-bit ALU
    Daddu,
    Dsubu,
    // shifts
    Sll,
    Srl,
    Sra,
    Sllv,
    Srlv,
    Srav,
    Dsll,
    Dsrl,
    Dsra,
    Dsll32,
    Dsrl32,
    Dsra32,
    // immediate (ADDI/DADDI match interpreter: overflow not trapped)
    Addiu,
    Andi,
    Ori,
    Xori,
    Lui,
    Slti,
    Sltiu,
    Daddiu,
    // jumps / branches (delay slot follows as next IR op(s))
    Jr,
    Jalr,
    J,
    Jal,
    Beq,
    Bne,
    Blez,
    Bgtz,
    Bltz,
    Bgez,
    Beql,
    Bnel,
    Blezl,
    Bgtzl,
    Bltzl,
    Bgezl,
    Bgezal,
    Bltzal,
    // memory
    Lb,
    Lbu,
    Lh,
    Lhu,
    Lw,
    Lwu,
    Ld,
    Lwl,
    Lwr,
    Sb,
    Sh,
    Sw,
    Sd,
    Swl,
    Swr,
    // hi/lo
    Mfhi,
    Mflo,
    Mthi,
    Mtlo,
    Mult,
    Multu,
    Div,
    Divu,
    // COP0
    Mfc0,
    Mtc0,
    Dmfc0,
    Dmtc0,
    // COP1 / FPU (helper-call; raw instruction in IrOp::target)
    Fpu,
    Bc1,
    Bc1l,
};

struct IrOp {
    IrOpKind kind{};
    uint8_t rd{0};
    uint8_t rs{0};
    uint8_t rt{0};
    uint8_t sa{0};
    uint16_t imm{0};
    // J/JAL target field, or raw instruction word for Fpu/Bc1/Bc1l helpers.
    uint32_t target{0};
};

struct IrBlock {
    uint32_t vaddr{0}; // guest VA of first instruction
    uint32_t paddr{0}; // physical address of first instruction
    std::vector<IrOp> ops;
    // true if block ends because of branch/jump (delay slot included)
    bool ends_with_branch{false};
};

} // namespace Jit
} // namespace Cpu
} // namespace N64

#endif

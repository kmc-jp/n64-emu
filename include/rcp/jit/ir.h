#ifndef RCP_JIT_IR_H
#define RCP_JIT_IR_H

#include <cstdint>
#include <vector>

namespace N64 {
namespace Rsp {
namespace Jit {

// IMEM loops are tiny; recompile is cheap.
constexpr int MAX_BLOCK_INSNS = 64;

// Scalar ops are emitted inline. Exec / mem helpers cover the rest.
enum class IrOpKind : uint8_t {
    Exec, // COP0 / COP2 moves / unknown → exec_one

    // Vector: PC updated in emitter, then thin helpers (no opcode re-decode).
    VuCompute, // COP2 compute (bit25) → vu_execute_compute
    Lwc2,      // → vu_load
    Swc2,      // → vu_store

    // R-type ALU
    Addu,
    Subu,
    And,
    Or,
    Xor,
    Nor,
    Slt,
    Sltu,
    Sll,
    Srl,
    Sra,
    Sllv,
    Srlv,
    Srav,

    // Immediate
    Addiu,
    Andi,
    Ori,
    Xori,
    Lui,
    Slti,
    Sltiu,

    // Branches / jumps (delay slot is the next IR op)
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
    Bltzal,
    Bgezal,
    Break,

    // DMEM via thin helpers (no opcode decode)
    Lb,
    Lbu,
    Lh,
    Lhu,
    Lw,
    Sb,
    Sh,
    Sw,
};

struct IrOp {
    IrOpKind kind{IrOpKind::Exec};
    uint32_t inst{0}; // for Exec
    uint8_t rd{0};
    uint8_t rs{0};
    uint8_t rt{0};
    uint8_t sa{0};
    uint16_t imm{0};    // raw 16-bit immediate
    uint16_t target{0}; // absolute next_pc for J/B* (post-advance relative)
};

struct IrBlock {
    uint16_t start_pc{0};
    std::vector<IrOp> ops;
};

} // namespace Jit
} // namespace Rsp
} // namespace N64

#endif

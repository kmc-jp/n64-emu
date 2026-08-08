#include "cpu/jit/jit.h"
#include "cpu/instruction.h"
#include "memory/bus.h"
#include "mmu/mmu.h"
#include <optional>

namespace N64 {
namespace Cpu {
namespace Jit {

namespace {

bool try_special(instruction_t inst, IrOp &op) {
    op.rs = static_cast<uint8_t>(inst.r_type.rs);
    op.rt = static_cast<uint8_t>(inst.r_type.rt);
    op.rd = static_cast<uint8_t>(inst.r_type.rd);
    op.sa = static_cast<uint8_t>(inst.r_type.sa);
    switch (inst.r_type.funct) {
    case SPECIAL_FUNCT_ADD:
        op.kind = IrOpKind::Add;
        return true;
    case SPECIAL_FUNCT_ADDU:
        op.kind = IrOpKind::Addu;
        return true;
    case SPECIAL_FUNCT_SUB:
        op.kind = IrOpKind::Sub;
        return true;
    case SPECIAL_FUNCT_SUBU:
        op.kind = IrOpKind::Subu;
        return true;
    case SPECIAL_FUNCT_AND:
        op.kind = IrOpKind::And;
        return true;
    case SPECIAL_FUNCT_OR:
        op.kind = IrOpKind::Or;
        return true;
    case SPECIAL_FUNCT_XOR:
        op.kind = IrOpKind::Xor;
        return true;
    case SPECIAL_FUNCT_NOR:
        op.kind = IrOpKind::Nor;
        return true;
    case SPECIAL_FUNCT_SLT:
        op.kind = IrOpKind::Slt;
        return true;
    case SPECIAL_FUNCT_SLTU:
        op.kind = IrOpKind::Sltu;
        return true;
    case SPECIAL_FUNCT_DADDU:
        op.kind = IrOpKind::Daddu;
        return true;
    case SPECIAL_FUNCT_DADD:
        // Overflow not trapped (same as interpreter TODO).
        op.kind = IrOpKind::Daddu;
        return true;
    case SPECIAL_FUNCT_DSUBU:
        op.kind = IrOpKind::Dsubu;
        return true;
    case SPECIAL_FUNCT_DSUB:
        op.kind = IrOpKind::Dsubu;
        return true;
    case SPECIAL_FUNCT_SLL:
        op.kind = IrOpKind::Sll;
        return true;
    case SPECIAL_FUNCT_SRL:
        op.kind = IrOpKind::Srl;
        return true;
    case SPECIAL_FUNCT_SRA:
        op.kind = IrOpKind::Sra;
        return true;
    case SPECIAL_FUNCT_SLLV:
        op.kind = IrOpKind::Sllv;
        return true;
    case SPECIAL_FUNCT_SRLV:
        op.kind = IrOpKind::Srlv;
        return true;
    case SPECIAL_FUNCT_SRAV:
        op.kind = IrOpKind::Srav;
        return true;
    case SPECIAL_FUNCT_DSLL:
        op.kind = IrOpKind::Dsll;
        return true;
    case SPECIAL_FUNCT_DSRL:
        op.kind = IrOpKind::Dsrl;
        return true;
    case SPECIAL_FUNCT_DSRA:
        op.kind = IrOpKind::Dsra;
        return true;
    case SPECIAL_FUNCT_DSLL32:
        op.kind = IrOpKind::Dsll32;
        return true;
    case SPECIAL_FUNCT_DSRL32:
        op.kind = IrOpKind::Dsrl32;
        return true;
    case SPECIAL_FUNCT_DSRA32:
        op.kind = IrOpKind::Dsra32;
        return true;
    case SPECIAL_FUNCT_JR:
        op.kind = IrOpKind::Jr;
        return true;
    case SPECIAL_FUNCT_JALR:
        op.kind = IrOpKind::Jalr;
        return true;
    case SPECIAL_FUNCT_MFHI:
        op.kind = IrOpKind::Mfhi;
        return true;
    case SPECIAL_FUNCT_MFLO:
        op.kind = IrOpKind::Mflo;
        return true;
    case SPECIAL_FUNCT_MTHI:
        op.kind = IrOpKind::Mthi;
        return true;
    case SPECIAL_FUNCT_MTLO:
        op.kind = IrOpKind::Mtlo;
        return true;
    case SPECIAL_FUNCT_MULT:
        op.kind = IrOpKind::Mult;
        return true;
    case SPECIAL_FUNCT_MULTU:
        op.kind = IrOpKind::Multu;
        return true;
    case SPECIAL_FUNCT_DIV:
        op.kind = IrOpKind::Div;
        return true;
    case SPECIAL_FUNCT_DIVU:
        op.kind = IrOpKind::Divu;
        return true;
    case SPECIAL_FUNCT_SYNC:
        op.kind = IrOpKind::Nop;
        return true;
    default:
        return false;
    }
}

bool try_regimm(instruction_t inst, IrOp &op) {
    op.rs = static_cast<uint8_t>(inst.i_type.rs);
    op.rt = static_cast<uint8_t>(inst.i_type.rt);
    op.imm = static_cast<uint16_t>(inst.i_type.imm);
    switch (inst.i_type.rt) {
    case REGIMM_RT_BLTZ:
        op.kind = IrOpKind::Bltz;
        return true;
    case REGIMM_RT_BGEZ:
        op.kind = IrOpKind::Bgez;
        return true;
    case REGIMM_RT_BLTZL:
        op.kind = IrOpKind::Bltzl;
        return true;
    case REGIMM_RT_BGEZL:
        op.kind = IrOpKind::Bgezl;
        return true;
    case REGIMM_RT_BLTZAL:
        op.kind = IrOpKind::Bltzal;
        return true;
    case REGIMM_RT_BGEZAL:
        op.kind = IrOpKind::Bgezal;
        return true;
    default:
        return false;
    }
}

bool try_cop0(instruction_t inst, IrOp &op) {
    // Only move to/from COP0 for now (no TLB ops / ERET).
    const uint8_t sub = static_cast<uint8_t>(inst.cop_r_like.sub);
    op.rt = static_cast<uint8_t>(inst.cop_r_like.rt);
    op.rd = static_cast<uint8_t>(inst.cop_r_like.rd);
    switch (sub) {
    case COP_MFC:
        op.kind = IrOpKind::Mfc0;
        return true;
    case COP_MTC:
        op.kind = IrOpKind::Mtc0;
        return true;
    case COP_DMFC:
        op.kind = IrOpKind::Dmfc0;
        return true;
    case COP_DMTC:
        op.kind = IrOpKind::Dmtc0;
        return true;
    default:
        return false;
    }
}

bool decode_one(uint32_t raw, IrOp &op) {
    instruction_t inst{};
    inst.raw = raw;
    op = {};
    switch (inst.op) {
    case OPCODE_SPECIAL:
        return try_special(inst, op);
    case OPCODE_REGIMM:
        return try_regimm(inst, op);
    case OPCODE_CP0:
        return try_cop0(inst, op);
    case OPCODE_J:
        op.kind = IrOpKind::J;
        op.target = inst.j_type.target;
        return true;
    case OPCODE_JAL:
        op.kind = IrOpKind::Jal;
        op.target = inst.j_type.target;
        return true;
    case OPCODE_ADDI:
        // Overflow not trapped (same as interpreter TODO).
        op.kind = IrOpKind::Addiu;
        break;
    case OPCODE_ADDIU:
        op.kind = IrOpKind::Addiu;
        break;
    case OPCODE_ANDI:
        op.kind = IrOpKind::Andi;
        break;
    case OPCODE_ORI:
        op.kind = IrOpKind::Ori;
        break;
    case OPCODE_XORI:
        op.kind = IrOpKind::Xori;
        break;
    case OPCODE_LUI:
        op.kind = IrOpKind::Lui;
        break;
    case OPCODE_SLTI:
        op.kind = IrOpKind::Slti;
        break;
    case OPCODE_SLTIU:
        op.kind = IrOpKind::Sltiu;
        break;
    case OPCODE_DADDI:
        op.kind = IrOpKind::Daddiu;
        break;
    case OPCODE_DADDIU:
        op.kind = IrOpKind::Daddiu;
        break;
    case OPCODE_BEQ:
        op.kind = IrOpKind::Beq;
        break;
    case OPCODE_BNE:
        op.kind = IrOpKind::Bne;
        break;
    case OPCODE_BLEZ:
        op.kind = IrOpKind::Blez;
        break;
    case OPCODE_BGTZ:
        op.kind = IrOpKind::Bgtz;
        break;
    case OPCODE_BEQL:
        op.kind = IrOpKind::Beql;
        break;
    case OPCODE_BNEL:
        op.kind = IrOpKind::Bnel;
        break;
    case OPCODE_BLEZL:
        op.kind = IrOpKind::Blezl;
        break;
    case OPCODE_BGTZL:
        op.kind = IrOpKind::Bgtzl;
        break;
    case OPCODE_LB:
        op.kind = IrOpKind::Lb;
        break;
    case OPCODE_LBU:
        op.kind = IrOpKind::Lbu;
        break;
    case OPCODE_LH:
        op.kind = IrOpKind::Lh;
        break;
    case OPCODE_LHU:
        op.kind = IrOpKind::Lhu;
        break;
    case OPCODE_LW:
        op.kind = IrOpKind::Lw;
        break;
    case OPCODE_LWU:
        op.kind = IrOpKind::Lwu;
        break;
    case OPCODE_LD:
        op.kind = IrOpKind::Ld;
        break;
    case OPCODE_LWL:
        op.kind = IrOpKind::Lwl;
        break;
    case OPCODE_LWR:
        op.kind = IrOpKind::Lwr;
        break;
    case OPCODE_SB:
        op.kind = IrOpKind::Sb;
        break;
    case OPCODE_SH:
        op.kind = IrOpKind::Sh;
        break;
    case OPCODE_SW:
        op.kind = IrOpKind::Sw;
        break;
    case OPCODE_SD:
        op.kind = IrOpKind::Sd;
        break;
    case OPCODE_SWL:
        op.kind = IrOpKind::Swl;
        break;
    case OPCODE_SWR:
        op.kind = IrOpKind::Swr;
        break;
    case OPCODE_LWC1:
    case OPCODE_LDC1:
    case OPCODE_SWC1:
    case OPCODE_SDC1:
        op.kind = IrOpKind::Fpu;
        op.target = raw;
        return true;
    case OPCODE_CP1: {
        if (inst.r_type.rs == COP_BC) {
            const uint8_t ndtf = static_cast<uint8_t>(inst.i_type.rt);
            op.kind = (ndtf == COP1_BC_FL || ndtf == COP1_BC_TL)
                          ? IrOpKind::Bc1l
                          : IrOpKind::Bc1;
        } else {
            op.kind = IrOpKind::Fpu;
        }
        op.target = raw;
        return true;
    }
    case OPCODE_CACHE:
        op.kind = IrOpKind::Nop;
        op.rs = static_cast<uint8_t>(inst.i_type.rs);
        op.rt = static_cast<uint8_t>(inst.i_type.rt);
        op.imm = static_cast<uint16_t>(inst.i_type.imm);
        return true;
    default:
        return false;
    }
    op.rs = static_cast<uint8_t>(inst.i_type.rs);
    op.rt = static_cast<uint8_t>(inst.i_type.rt);
    op.imm = static_cast<uint16_t>(inst.i_type.imm);
    return true;
}

bool is_branch(IrOpKind k) {
    switch (k) {
    case IrOpKind::Jr:
    case IrOpKind::Jalr:
    case IrOpKind::J:
    case IrOpKind::Jal:
    case IrOpKind::Beq:
    case IrOpKind::Bne:
    case IrOpKind::Blez:
    case IrOpKind::Bgtz:
    case IrOpKind::Bltz:
    case IrOpKind::Bgez:
    case IrOpKind::Beql:
    case IrOpKind::Bnel:
    case IrOpKind::Blezl:
    case IrOpKind::Bgtzl:
    case IrOpKind::Bltzl:
    case IrOpKind::Bgezl:
    case IrOpKind::Bgezal:
    case IrOpKind::Bltzal:
    case IrOpKind::Bc1:
    case IrOpKind::Bc1l:
        return true;
    default:
        return false;
    }
}

} // namespace

bool translate_block(uint32_t vaddr, uint32_t paddr, IrBlock &out) {
    out = {};
    out.vaddr = vaddr;
    out.paddr = paddr;

    uint32_t cur_v = vaddr;
    uint32_t cur_p = paddr;
    const uint32_t start_page = paddr & ~0xFFFu;

    while (static_cast<int>(out.ops.size()) < MAX_BLOCK_INSNS) {
        // Stop at physical page boundary (except for delay slot).
        if ((cur_p & ~0xFFFu) != start_page && !out.ends_with_branch)
            break;

        const uint32_t raw = Memory::read_paddr32(cur_p);
        IrOp op{};
        if (!decode_one(raw, op)) {
            // Unsupported: if nothing translated yet, fail; else end block.
            if (out.ops.empty())
                return false;
            break;
        }

        out.ops.push_back(op);
        cur_v += 4;
        cur_p += 4;

        if (is_branch(op.kind)) {
            out.ends_with_branch = true;
            // Include delay slot.
            if (static_cast<int>(out.ops.size()) >= MAX_BLOCK_INSNS)
                break;
            if ((cur_p & ~0xFFFu) != start_page) {
                // Delay slot on next page — still include if possible.
            }
            const uint32_t ds_raw = Memory::read_paddr32(cur_p);
            IrOp ds{};
            if (!decode_one(ds_raw, ds)) {
                // Cannot compile delay slot; drop the branch too if it was
                // the only way — force interpreter for this PC.
                out.ops.pop_back();
                return !out.ops.empty();
            }
            out.ops.push_back(ds);
            break;
        }
    }

    return !out.ops.empty();
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

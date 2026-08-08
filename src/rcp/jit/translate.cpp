#include "rcp/jit/jit.h"
#include "rcp/rsp.h"
#include "utils/byte_array.h"
#include <cstdint>

namespace N64 {
namespace Rsp {
namespace Jit {

namespace {

uint8_t opcode(uint32_t inst) {
    return static_cast<uint8_t>((inst >> 26) & 0x3F);
}
uint8_t rs_f(uint32_t inst) {
    return static_cast<uint8_t>((inst >> 21) & 0x1F);
}
uint8_t rt_f(uint32_t inst) {
    return static_cast<uint8_t>((inst >> 16) & 0x1F);
}
uint8_t rd_f(uint32_t inst) {
    return static_cast<uint8_t>((inst >> 11) & 0x1F);
}
uint8_t sa_f(uint32_t inst) {
    return static_cast<uint8_t>((inst >> 6) & 0x1F);
}
uint8_t funct(uint32_t inst) { return static_cast<uint8_t>(inst & 0x3F); }
uint16_t imm16(uint32_t inst) { return static_cast<uint16_t>(inst & 0xFFFF); }
int16_t imm_se(uint32_t inst) { return static_cast<int16_t>(inst & 0xFFFF); }
uint32_t jtgt(uint32_t inst) { return (inst & 0x03FFFFFF) << 2; }

bool is_break(uint32_t inst) {
    return opcode(inst) == 0x00 && funct(inst) == 0x0D;
}

bool is_branch_or_jump(uint32_t inst) {
    const uint8_t op = opcode(inst);
    switch (op) {
    case 0x01: // REGIMM
    case 0x02: // J
    case 0x03: // JAL
    case 0x04: // BEQ
    case 0x05: // BNE
    case 0x06: // BLEZ
    case 0x07: // BGTZ
        return true;
    case 0x00:
        return funct(inst) == 0x08 || funct(inst) == 0x09;
    default:
        return false;
    }
}

// After jit_step advance, guest pc points at the delay slot (= inst_pc+4).
uint16_t branch_target(uint16_t inst_pc, int16_t off) {
    const uint16_t delay = static_cast<uint16_t>((inst_pc + 4) & 0xFFC);
    return static_cast<uint16_t>(delay + static_cast<uint16_t>(off << 2)) &
           0xFFC;
}

uint16_t link_addr(uint16_t inst_pc) {
    // Link = instruction after the delay slot.
    return static_cast<uint16_t>((inst_pc + 8) & 0xFFC);
}

IrOp make_exec(uint32_t inst) {
    IrOp op{};
    op.kind = IrOpKind::Exec;
    op.inst = inst;
    return op;
}

IrOp decode(uint32_t inst, uint16_t inst_pc) {
    IrOp op{};
    op.inst = inst;
    op.rs = rs_f(inst);
    op.rt = rt_f(inst);
    op.rd = rd_f(inst);
    op.sa = sa_f(inst);
    op.imm = imm16(inst);

    const uint8_t opc = opcode(inst);
    switch (opc) {
    case 0x00: { // SPECIAL
        switch (funct(inst)) {
        case 0x00:
            op.kind = IrOpKind::Sll;
            return op;
        case 0x02:
            op.kind = IrOpKind::Srl;
            return op;
        case 0x03:
            op.kind = IrOpKind::Sra;
            return op;
        case 0x04:
            op.kind = IrOpKind::Sllv;
            return op;
        case 0x06:
            op.kind = IrOpKind::Srlv;
            return op;
        case 0x07:
            op.kind = IrOpKind::Srav;
            return op;
        case 0x08:
            op.kind = IrOpKind::Jr;
            return op;
        case 0x09:
            op.kind = IrOpKind::Jalr;
            op.target = link_addr(inst_pc); // baked link value
            return op;
        case 0x0D:
            op.kind = IrOpKind::Break;
            return op;
        case 0x20:
        case 0x21:
            op.kind = IrOpKind::Addu;
            return op;
        case 0x22:
        case 0x23:
            op.kind = IrOpKind::Subu;
            return op;
        case 0x24:
            op.kind = IrOpKind::And;
            return op;
        case 0x25:
            op.kind = IrOpKind::Or;
            return op;
        case 0x26:
            op.kind = IrOpKind::Xor;
            return op;
        case 0x27:
            op.kind = IrOpKind::Nor;
            return op;
        case 0x2A:
            op.kind = IrOpKind::Slt;
            return op;
        case 0x2B:
            op.kind = IrOpKind::Sltu;
            return op;
        default:
            return make_exec(inst);
        }
    }
    case 0x01: { // REGIMM
        op.target = branch_target(inst_pc, imm_se(inst));
        switch (rt_f(inst)) {
        case 0x00:
            op.kind = IrOpKind::Bltz;
            return op;
        case 0x01:
            op.kind = IrOpKind::Bgez;
            return op;
        case 0x10:
            op.kind = IrOpKind::Bltzal;
            op.rd = 31;
            op.sa = 0; // unused; link baked in target field of Jalr style
            // Store link in imm high? Use dedicated: put link in inst's unused
            // — use `imm` as link low and keep branch target in `target`.
            op.imm = link_addr(inst_pc);
            return op;
        case 0x11:
            op.kind = IrOpKind::Bgezal;
            op.imm = link_addr(inst_pc);
            return op;
        default:
            return make_exec(inst);
        }
    }
    case 0x02:
        op.kind = IrOpKind::J;
        op.target = static_cast<uint16_t>(jtgt(inst)) & 0xFFC;
        return op;
    case 0x03:
        op.kind = IrOpKind::Jal;
        op.target = static_cast<uint16_t>(jtgt(inst)) & 0xFFC;
        op.imm = link_addr(inst_pc);
        return op;
    case 0x04:
        op.kind = IrOpKind::Beq;
        op.target = branch_target(inst_pc, imm_se(inst));
        return op;
    case 0x05:
        op.kind = IrOpKind::Bne;
        op.target = branch_target(inst_pc, imm_se(inst));
        return op;
    case 0x06:
        op.kind = IrOpKind::Blez;
        op.target = branch_target(inst_pc, imm_se(inst));
        return op;
    case 0x07:
        op.kind = IrOpKind::Bgtz;
        op.target = branch_target(inst_pc, imm_se(inst));
        return op;
    case 0x08:
    case 0x09:
        op.kind = IrOpKind::Addiu;
        return op;
    case 0x0A:
        op.kind = IrOpKind::Slti;
        return op;
    case 0x0B:
        op.kind = IrOpKind::Sltiu;
        return op;
    case 0x0C:
        op.kind = IrOpKind::Andi;
        return op;
    case 0x0D:
        op.kind = IrOpKind::Ori;
        return op;
    case 0x0E:
        op.kind = IrOpKind::Xori;
        return op;
    case 0x0F:
        op.kind = IrOpKind::Lui;
        return op;
    case 0x20:
        op.kind = IrOpKind::Lb;
        return op;
    case 0x21:
        op.kind = IrOpKind::Lh;
        return op;
    case 0x23:
        op.kind = IrOpKind::Lw;
        return op;
    case 0x24:
        op.kind = IrOpKind::Lbu;
        return op;
    case 0x25:
        op.kind = IrOpKind::Lhu;
        return op;
    case 0x28:
        op.kind = IrOpKind::Sb;
        return op;
    case 0x29:
        op.kind = IrOpKind::Sh;
        return op;
    case 0x2B:
        op.kind = IrOpKind::Sw;
        return op;
    case 0x12: // COP2
        if (inst & (1u << 25)) {
            op.kind = IrOpKind::VuCompute;
            op.rd = static_cast<uint8_t>((inst >> 6) & 0x1F);   // vd
            op.rs = static_cast<uint8_t>((inst >> 11) & 0x1F);  // vs
            op.rt = static_cast<uint8_t>((inst >> 16) & 0x1F);  // vt
            op.sa = static_cast<uint8_t>((inst >> 21) & 0xF);   // e
            op.imm = static_cast<uint16_t>(inst & 0x3F);        // funct
            return op;
        }
        return make_exec(inst); // MFC2/MTC2/CFC2/CTC2
    case 0x32:
        op.kind = IrOpKind::Lwc2;
        return op;
    case 0x3A:
        op.kind = IrOpKind::Swc2;
        return op;
    case 0x10: // COP0
    default:
        return make_exec(inst);
    }
}

} // namespace

bool translate_block(uint16_t start_pc, IrBlock &out) {
    out.start_pc = start_pc & 0xFFC;
    out.ops.clear();
    out.ops.reserve(16);

    auto &imem = g_rsp().get_sp_imem();
    uint16_t pc = out.start_pc;
    bool include_delay = false;

    for (int n = 0; n < MAX_BLOCK_INSNS; n++) {
        const uint32_t inst = Utils::read_from_byte_array32(imem, pc & 0xFFC);
        out.ops.push_back(decode(inst, pc));

        if (include_delay)
            break;
        if (is_break(inst))
            break;
        if (is_branch_or_jump(inst)) {
            include_delay = true;
            pc = static_cast<uint16_t>((pc + 4) & 0xFFC);
            continue;
        }
        pc = static_cast<uint16_t>((pc + 4) & 0xFFC);
    }

    return !out.ops.empty();
}

} // namespace Jit
} // namespace Rsp
} // namespace N64

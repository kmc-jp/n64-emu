#include "cpu/jit/code_cache.h"
#include "cpu/jit/helpers.h"
#include "cpu/jit/ir.h"
#include "cpu/jit/jit.h"
#include <xbyak/xbyak.h>
#include <cstddef>
#include <cstring>

namespace N64 {
namespace Cpu {
namespace Jit {

namespace {

using namespace Xbyak;
using namespace Xbyak::util;

class BlockEmitter : public CodeGenerator {
  public:
    explicit BlockEmitter(uint8_t *buf, size_t size)
        : CodeGenerator(size, buf) {}

    BlockFn emit(const IrBlock &block) {
        const size_t n = block.ops.size();
        Xbyak::Label exit_label;

        // prologue: keep cycles in ebx (callee-saved).
        // On entry RSP is 8-mod-16; after two pushes still 8-mod-16.
        // ABI requires 16-byte alignment before CALL.
        push(rbx);
        push(r12);
        sub(rsp, 8);
        xor_(ebx, ebx); // cycles_done

        for (size_t i = 0; i < n; i++) {
            // advance_pc()
            mov(rax, reinterpret_cast<uintptr_t>(&advance_pc));
            call(rax);

            emit_op(block.ops[i], exit_label);

            // cycles_done++
            inc(ebx);

            // if (exec_state().aborted) goto exit
            mov(rax, reinterpret_cast<uintptr_t>(&exec_state));
            call(rax);
            // rax = &ExecState
            cmp(byte[rax + offsetof(ExecState, aborted)], 0);
            jne(exit_label, T_NEAR);
        }

        L(exit_label);
        // add_count(cycles)
        mov(edi, ebx);
        mov(rax, reinterpret_cast<uintptr_t>(&add_count));
        call(rax);

        mov(eax, ebx); // return cycles
        add(rsp, 8);
        pop(r12);
        pop(rbx);
        ret();

        ready();
        return getCode<BlockFn>();
    }

  private:
    void call_fn(const void *fn) {
        mov(rax, reinterpret_cast<uintptr_t>(fn));
        call(rax);
    }

    void gpr_to_rax(uint8_t reg) {
        mov(edi, reg);
        call_fn(reinterpret_cast<const void *>(&gpr_get));
    }

    void rax_to_gpr(uint8_t reg) {
        // value in rax
        mov(rsi, rax);
        mov(edi, reg);
        call_fn(reinterpret_cast<const void *>(&gpr_set));
    }

    void emit_alu_rr_32(IrOpKind kind, const IrOp &op) {
        // rs -> r12, rt -> rax; 32-bit arithmetic then sign-extend
        gpr_to_rax(op.rs);
        mov(r12, rax);
        gpr_to_rax(op.rt);
        mov(ecx, eax);  // rt
        mov(eax, r12d); // rs

        switch (kind) {
        case IrOpKind::Add:
        case IrOpKind::Addu:
            add(eax, ecx);
            break;
        case IrOpKind::Sub:
        case IrOpKind::Subu:
            sub(eax, ecx);
            break;
        default:
            break;
        }
        cdqe();
        rax_to_gpr(op.rd);
    }

    void emit_alu_rr_64_logic(IrOpKind kind, const IrOp &op) {
        gpr_to_rax(op.rs);
        mov(r12, rax);
        gpr_to_rax(op.rt);
        switch (kind) {
        case IrOpKind::And:
            and_(rax, r12);
            break;
        case IrOpKind::Or:
            or_(rax, r12);
            break;
        case IrOpKind::Xor:
            xor_(rax, r12);
            break;
        case IrOpKind::Nor:
            or_(rax, r12);
            not_(rax);
            break;
        default:
            break;
        }
        rax_to_gpr(op.rd);
    }

    void emit_slt(IrOpKind kind, const IrOp &op) {
        gpr_to_rax(op.rs);
        mov(r12, rax);
        gpr_to_rax(op.rt);
        cmp(r12, rax);
        if (kind == IrOpKind::Slt)
            setl(al);
        else
            setb(al);
        movzx(eax, al);
        cdqe();
        rax_to_gpr(op.rd);
    }

    void emit_shift_sa(IrOpKind kind, const IrOp &op) {
        gpr_to_rax(op.rt);
        mov(ecx, op.sa);
        switch (kind) {
        case IrOpKind::Sll:
            shl(eax, cl);
            cdqe();
            break;
        case IrOpKind::Srl:
            shr(eax, cl);
            cdqe();
            break;
        case IrOpKind::Sra:
            sar(eax, cl);
            cdqe();
            break;
        case IrOpKind::Dsll:
            shl(rax, cl);
            break;
        case IrOpKind::Dsrl:
            shr(rax, cl);
            break;
        case IrOpKind::Dsra:
            sar(rax, cl);
            break;
        case IrOpKind::Dsll32:
            mov(ecx, op.sa + 32);
            shl(rax, cl);
            break;
        case IrOpKind::Dsrl32:
            mov(ecx, op.sa + 32);
            shr(rax, cl);
            break;
        case IrOpKind::Dsra32:
            mov(ecx, op.sa + 32);
            sar(rax, cl);
            break;
        default:
            break;
        }
        rax_to_gpr(op.rd);
    }

    void emit_shift_v(IrOpKind kind, const IrOp &op) {
        gpr_to_rax(op.rs);
        mov(ecx, eax); // shift amount
        and_(ecx, 31);
        gpr_to_rax(op.rt);
        switch (kind) {
        case IrOpKind::Sllv:
            shl(eax, cl);
            cdqe();
            break;
        case IrOpKind::Srlv:
            shr(eax, cl);
            cdqe();
            break;
        case IrOpKind::Srav:
            sar(eax, cl);
            cdqe();
            break;
        default:
            break;
        }
        rax_to_gpr(op.rd);
    }

    void emit_imm(IrOpKind kind, const IrOp &op) {
        const int16_t simm = static_cast<int16_t>(op.imm);
        const uint16_t zimm = op.imm;
        gpr_to_rax(op.rs);
        switch (kind) {
        case IrOpKind::Addiu: {
            add(eax, simm);
            cdqe();
            break;
        }
        case IrOpKind::Daddiu: {
            mov(rcx, static_cast<int64_t>(simm));
            add(rax, rcx);
            break;
        }
        case IrOpKind::Andi: {
            and_(rax, static_cast<uint64_t>(zimm));
            break;
        }
        case IrOpKind::Ori: {
            or_(rax, static_cast<uint64_t>(zimm));
            break;
        }
        case IrOpKind::Xori: {
            xor_(rax, static_cast<uint64_t>(zimm));
            break;
        }
        case IrOpKind::Lui: {
            mov(eax, static_cast<uint32_t>(zimm) << 16);
            cdqe();
            break;
        }
        case IrOpKind::Slti: {
            mov(rcx, static_cast<int64_t>(simm));
            cmp(rax, rcx);
            setl(al);
            movzx(eax, al);
            cdqe();
            break;
        }
        case IrOpKind::Sltiu: {
            mov(rcx, static_cast<uint64_t>(static_cast<uint16_t>(simm)));
            // SLTIU uses sign-extended imm compared as unsigned
            mov(rcx, static_cast<int64_t>(simm));
            cmp(rax, rcx);
            setb(al);
            movzx(eax, al);
            cdqe();
            break;
        }
        default:
            break;
        }
        rax_to_gpr(op.rt);
    }

    void emit_branch(const IrOp &op) {
        const int16_t off = static_cast<int16_t>(op.imm);
        switch (op.kind) {
        case IrOpKind::Jr:
            gpr_to_rax(op.rs);
            mov(rsi, rax);
            mov(edi, 1);
            call_fn(reinterpret_cast<const void *>(&do_branch_addr));
            break;
        case IrOpKind::Jalr:
            gpr_to_rax(op.rs);
            mov(r12, rax);
            mov(edi, 1);
            mov(rsi, r12);
            call_fn(reinterpret_cast<const void *>(&do_branch_addr));
            mov(edi, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_link));
            break;
        case IrOpKind::J: {
            // target = (pc & 0xFFFFFFFF'F0000000) | (target << 2)
            // After advance_pc, cpu.pc is the delay-slot address (= old_pc+4).
            // Interpreter uses (pc & mask) for J where pc was already updated.
            call_fn(reinterpret_cast<const void *>(&get_pc));
            mov(rcx, 0xFFFFFFFFF0000000ULL);
            and_(rax, rcx);
            mov(rcx, static_cast<uint64_t>(op.target) << 2);
            or_(rax, rcx);
            mov(rsi, rax);
            mov(edi, 1);
            call_fn(reinterpret_cast<const void *>(&do_branch_addr));
            break;
        }
        case IrOpKind::Jal: {
            call_fn(reinterpret_cast<const void *>(&get_pc));
            mov(r12, rax);
            mov(rcx, 0xFFFFFFFFF0000000ULL);
            and_(rax, rcx);
            mov(rcx, static_cast<uint64_t>(op.target) << 2);
            or_(rax, rcx);
            mov(rsi, rax);
            mov(edi, 1);
            call_fn(reinterpret_cast<const void *>(&do_branch_addr));
            mov(edi, 31);
            call_fn(reinterpret_cast<const void *>(&do_link));
            break;
        }
        case IrOpKind::Beq:
        case IrOpKind::Beql:
        case IrOpKind::Bne:
        case IrOpKind::Bnel: {
            gpr_to_rax(op.rs);
            mov(r12, rax);
            gpr_to_rax(op.rt);
            cmp(r12, rax);
            setz(al);
            movzx(edi, al);
            if (op.kind == IrOpKind::Bne || op.kind == IrOpKind::Bnel)
                xor_(edi, 1);
            mov(esi, off);
            if (op.kind == IrOpKind::Beql || op.kind == IrOpKind::Bnel)
                call_fn(reinterpret_cast<const void *>(&do_branch_likely_offset));
            else
                call_fn(reinterpret_cast<const void *>(&do_branch_offset));
            break;
        }
        case IrOpKind::Blez:
        case IrOpKind::Blezl:
        case IrOpKind::Bgtz:
        case IrOpKind::Bgtzl: {
            gpr_to_rax(op.rs);
            test(rax, rax);
            if (op.kind == IrOpKind::Blez || op.kind == IrOpKind::Blezl)
                setle(al);
            else
                setg(al);
            movzx(edi, al);
            mov(esi, off);
            if (op.kind == IrOpKind::Blezl || op.kind == IrOpKind::Bgtzl)
                call_fn(reinterpret_cast<const void *>(&do_branch_likely_offset));
            else
                call_fn(reinterpret_cast<const void *>(&do_branch_offset));
            break;
        }
        case IrOpKind::Bltz:
        case IrOpKind::Bltzl:
        case IrOpKind::Bgez:
        case IrOpKind::Bgezl:
        case IrOpKind::Bltzal:
        case IrOpKind::Bgezal: {
            gpr_to_rax(op.rs);
            test(rax, rax);
            if (op.kind == IrOpKind::Bltz || op.kind == IrOpKind::Bltzl ||
                op.kind == IrOpKind::Bltzal)
                setl(al);
            else
                setge(al);
            movzx(edi, al);
            mov(esi, off);
            const bool likely = op.kind == IrOpKind::Bltzl ||
                                op.kind == IrOpKind::Bgezl;
            if (likely)
                call_fn(reinterpret_cast<const void *>(&do_branch_likely_offset));
            else
                call_fn(reinterpret_cast<const void *>(&do_branch_offset));
            if (op.kind == IrOpKind::Bltzal || op.kind == IrOpKind::Bgezal) {
                mov(edi, 31);
                call_fn(reinterpret_cast<const void *>(&do_link));
            }
            break;
        }
        default:
            break;
        }
    }

    void emit_mem(const IrOp &op) {
        mov(edi, op.rt);
        mov(esi, op.rs);
        mov(edx, static_cast<int16_t>(op.imm));
        const void *fn = nullptr;
        switch (op.kind) {
        case IrOpKind::Lb:
            fn = reinterpret_cast<const void *>(&do_lb);
            break;
        case IrOpKind::Lbu:
            fn = reinterpret_cast<const void *>(&do_lbu);
            break;
        case IrOpKind::Lh:
            fn = reinterpret_cast<const void *>(&do_lh);
            break;
        case IrOpKind::Lhu:
            fn = reinterpret_cast<const void *>(&do_lhu);
            break;
        case IrOpKind::Lw:
            fn = reinterpret_cast<const void *>(&do_lw);
            break;
        case IrOpKind::Lwu:
            fn = reinterpret_cast<const void *>(&do_lwu);
            break;
        case IrOpKind::Ld:
            fn = reinterpret_cast<const void *>(&do_ld);
            break;
        case IrOpKind::Sb:
            fn = reinterpret_cast<const void *>(&do_sb);
            break;
        case IrOpKind::Sh:
            fn = reinterpret_cast<const void *>(&do_sh);
            break;
        case IrOpKind::Sw:
            fn = reinterpret_cast<const void *>(&do_sw);
            break;
        case IrOpKind::Sd:
            fn = reinterpret_cast<const void *>(&do_sd);
            break;
        default:
            break;
        }
        if (fn)
            call_fn(fn);
    }

    void emit_op(const IrOp &op, Xbyak::Label & /*exit_label*/) {
        switch (op.kind) {
        case IrOpKind::Nop:
            break;
        case IrOpKind::Add:
        case IrOpKind::Addu:
        case IrOpKind::Sub:
        case IrOpKind::Subu:
            emit_alu_rr_32(op.kind, op);
            break;
        case IrOpKind::And:
        case IrOpKind::Or:
        case IrOpKind::Xor:
        case IrOpKind::Nor:
            emit_alu_rr_64_logic(op.kind, op);
            break;
        case IrOpKind::Slt:
        case IrOpKind::Sltu:
            emit_slt(op.kind, op);
            break;
        case IrOpKind::Daddu: {
            gpr_to_rax(op.rs);
            mov(r12, rax);
            gpr_to_rax(op.rt);
            add(rax, r12);
            rax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Dsubu: {
            gpr_to_rax(op.rs);
            mov(r12, rax);
            gpr_to_rax(op.rt);
            mov(rcx, rax);
            mov(rax, r12);
            sub(rax, rcx);
            rax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Sll:
        case IrOpKind::Srl:
        case IrOpKind::Sra:
        case IrOpKind::Dsll:
        case IrOpKind::Dsrl:
        case IrOpKind::Dsra:
        case IrOpKind::Dsll32:
        case IrOpKind::Dsrl32:
        case IrOpKind::Dsra32:
            emit_shift_sa(op.kind, op);
            break;
        case IrOpKind::Sllv:
        case IrOpKind::Srlv:
        case IrOpKind::Srav:
            emit_shift_v(op.kind, op);
            break;
        case IrOpKind::Addiu:
        case IrOpKind::Andi:
        case IrOpKind::Ori:
        case IrOpKind::Xori:
        case IrOpKind::Lui:
        case IrOpKind::Slti:
        case IrOpKind::Sltiu:
        case IrOpKind::Daddiu:
            emit_imm(op.kind, op);
            break;
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
            emit_branch(op);
            break;
        case IrOpKind::Lb:
        case IrOpKind::Lbu:
        case IrOpKind::Lh:
        case IrOpKind::Lhu:
        case IrOpKind::Lw:
        case IrOpKind::Lwu:
        case IrOpKind::Ld:
        case IrOpKind::Sb:
        case IrOpKind::Sh:
        case IrOpKind::Sw:
        case IrOpKind::Sd:
            emit_mem(op);
            break;
        case IrOpKind::Mfhi:
            call_fn(reinterpret_cast<const void *>(&get_hi));
            rax_to_gpr(op.rd);
            break;
        case IrOpKind::Mflo:
            call_fn(reinterpret_cast<const void *>(&get_lo));
            rax_to_gpr(op.rd);
            break;
        case IrOpKind::Mthi:
            gpr_to_rax(op.rs);
            mov(rdi, rax);
            call_fn(reinterpret_cast<const void *>(&set_hi));
            break;
        case IrOpKind::Mtlo:
            gpr_to_rax(op.rs);
            mov(rdi, rax);
            call_fn(reinterpret_cast<const void *>(&set_lo));
            break;
        case IrOpKind::Mfc0:
            mov(edi, op.rt);
            mov(esi, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_mfc0));
            break;
        case IrOpKind::Mtc0:
            mov(edi, op.rt);
            mov(esi, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_mtc0));
            break;
        case IrOpKind::Dmfc0:
            mov(edi, op.rt);
            mov(esi, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_dmfc0));
            break;
        case IrOpKind::Dmtc0:
            mov(edi, op.rt);
            mov(esi, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_dmtc0));
            break;
        }
    }
};

} // namespace

BlockFn emit_block(const IrBlock &block, CodeCache &cache) {
    constexpr size_t kBufSize = 64 * 1024;
    uint8_t *buf = cache.alloc_exec(kBufSize);
    BlockEmitter emitter(buf, kBufSize);
    return emitter.emit(block);
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

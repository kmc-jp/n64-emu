#include "cpu/jit/code_cache.h"
#include "cpu/cpu.h"
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

// Host C++ calling convention for helper calls from emitted code.
#ifdef _WIN32
// Microsoft x64: RCX, RDX, R8, R9 + 32-byte shadow space.
static constexpr size_t kAbiStackAdjust = 0x28; // 8-byte align + 32 shadow
#define JIT_ARG1d ecx
#define JIT_ARG1q rcx
#define JIT_ARG2d edx
#define JIT_ARG2q rdx
#define JIT_ARG3d r8d
#else
// System V: RDI, RSI, RDX, RCX, … (no shadow space).
static constexpr size_t kAbiStackAdjust = 8;
#define JIT_ARG1d edi
#define JIT_ARG1q rdi
#define JIT_ARG2d esi
#define JIT_ARG2q rsi
#define JIT_ARG3d edx
#endif

bool is_branch_likely(IrOpKind k) {
    switch (k) {
    case IrOpKind::Beql:
    case IrOpKind::Bnel:
    case IrOpKind::Blezl:
    case IrOpKind::Bgtzl:
    case IrOpKind::Bltzl:
    case IrOpKind::Bgezl:
        return true;
    default:
        return false;
    }
}

bool is_mem_op(IrOpKind k) {
    switch (k) {
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
        return true;
    default:
        return false;
    }
}

class BlockEmitter : public CodeGenerator {
  public:
    explicit BlockEmitter(uint8_t *buf, size_t size)
        : CodeGenerator(size, buf) {
        auto &cpu = g_cpu();
        gpr_base_ = reinterpret_cast<uintptr_t>(cpu.gpr_data());
        lo_ptr_ = reinterpret_cast<uintptr_t>(cpu.lo_ptr());
        hi_ptr_ = reinterpret_cast<uintptr_t>(cpu.hi_ptr());
        delay_slot_ptr_ = reinterpret_cast<uintptr_t>(cpu.delay_slot_ptr());
        prev_delay_slot_ptr_ =
            reinterpret_cast<uintptr_t>(cpu.prev_delay_slot_ptr());
        prev_pc_ptr_ = reinterpret_cast<uintptr_t>(cpu.prev_pc_ptr());
        pc_ptr_ = reinterpret_cast<uintptr_t>(cpu.pc_ptr());
        next_pc_ptr_ = reinterpret_cast<uintptr_t>(cpu.next_pc_ptr());
        aborted_ptr_ =
            reinterpret_cast<uintptr_t>(&exec_state_ptr()->aborted);
        annul_ptr_ =
            reinterpret_cast<uintptr_t>(&exec_state_ptr()->annul_delay_slot);
    }

    BlockFn emit(const IrBlock &block) {
        const size_t n = block.ops.size();
        Xbyak::Label exit_label;

        // prologue: keep cycles in ebx (callee-saved).
        // On entry RSP is 8-mod-16; after two pushes still 8-mod-16.
        // ABI requires 16-byte alignment before CALL (Win64 also needs shadow).
        push(rbx);
        push(r12);
        sub(rsp, kAbiStackAdjust);
        xor_(ebx, ebx); // cycles_done

        for (size_t i = 0; i < n; i++) {
            emit_advance_pc();

            emit_op(block.ops[i], exit_label);

            // cycles_done++
            inc(ebx);

            // Only memory ops can set aborted today; skip the check elsewhere.
            if (is_mem_op(block.ops[i].kind)) {
                mov(rax, aborted_ptr_);
                cmp(byte[rax], 0);
                jne(exit_label, T_NEAR);
            }

            // Branch-likely may annul the delay slot that follows in this block.
            if (is_branch_likely(block.ops[i].kind) && i + 1 < n) {
                mov(rax, annul_ptr_);
                cmp(byte[rax], 0);
                jne(exit_label, T_NEAR);
            }
        }

        L(exit_label);
        // add_count(cycles) — compare-edge logic stays in C++.
        mov(JIT_ARG1d, ebx);
        mov(rax, reinterpret_cast<uintptr_t>(&add_count));
        call(rax);

        mov(eax, ebx); // return cycles
        add(rsp, kAbiStackAdjust);
        pop(r12);
        pop(rbx);
        ret();

        ready();
        return getCode<BlockFn>();
    }

  private:
    uintptr_t gpr_base_{};
    uintptr_t lo_ptr_{};
    uintptr_t hi_ptr_{};
    uintptr_t delay_slot_ptr_{};
    uintptr_t prev_delay_slot_ptr_{};
    uintptr_t prev_pc_ptr_{};
    uintptr_t pc_ptr_{};
    uintptr_t next_pc_ptr_{};
    uintptr_t aborted_ptr_{};
    uintptr_t annul_ptr_{};

    void call_fn(const void *fn) {
        mov(rax, reinterpret_cast<uintptr_t>(fn));
        call(rax);
    }

    // Inline Cpu::advance_pc_no_fetch().
    void emit_advance_pc() {
        // prev_delay_slot = delay_slot; delay_slot = false;
        mov(rax, delay_slot_ptr_);
        movzx(ecx, byte[rax]);
        mov(rdx, prev_delay_slot_ptr_);
        mov(byte[rdx], cl);
        mov(byte[rax], 0);

        // prev_pc = pc; pc = next_pc; next_pc += 4;
        mov(rax, pc_ptr_);
        mov(rcx, qword[rax]);
        mov(rdx, prev_pc_ptr_);
        mov(qword[rdx], rcx);
        mov(rdx, next_pc_ptr_);
        mov(rcx, qword[rdx]);
        mov(qword[rax], rcx);
        add(qword[rdx], 4);
    }

    void gpr_to_rax(uint8_t reg) {
        if (reg == 0) {
            xor_(eax, eax);
            return;
        }
        mov(rax, gpr_base_ + static_cast<uintptr_t>(reg) * 8);
        mov(rax, qword[rax]);
    }

    void rax_to_gpr(uint8_t reg) {
        if (reg == 0)
            return;
        // value in rax
        mov(rdx, gpr_base_ + static_cast<uintptr_t>(reg) * 8);
        mov(qword[rdx], rax);
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

    // Inline Cpu::branch_addr64 for a PC-relative offset (non-likely).
    // Condition is already in JIT_ARG1d (0/1). Uses pc/next_pc/delay_slot ptrs.
    void emit_branch_offset_inline(int16_t off) {
        Xbyak::Label not_taken, done;
        // delay_slot = true
        mov(rax, delay_slot_ptr_);
        mov(byte[rax], 1);
        test(JIT_ARG1d, JIT_ARG1d);
        jz(not_taken, T_NEAR);
        // next_pc = pc + (int64_t)off * 4
        mov(rax, pc_ptr_);
        mov(rcx, qword[rax]);
        add(rcx, static_cast<int64_t>(off) * 4);
        mov(rax, next_pc_ptr_);
        mov(qword[rax], rcx);
        jmp(done, T_NEAR);
        L(not_taken);
        // not taken: next_pc already points at fall-through
        L(done);
    }

    void emit_branch(const IrOp &op) {
        const int16_t off = static_cast<int16_t>(op.imm);
        switch (op.kind) {
        case IrOpKind::Jr:
            gpr_to_rax(op.rs);
            mov(JIT_ARG2q, rax);
            mov(JIT_ARG1d, 1);
            call_fn(reinterpret_cast<const void *>(&do_branch_addr));
            break;
        case IrOpKind::Jalr:
            gpr_to_rax(op.rs);
            mov(r12, rax);
            mov(JIT_ARG1d, 1);
            mov(JIT_ARG2q, r12);
            call_fn(reinterpret_cast<const void *>(&do_branch_addr));
            mov(JIT_ARG1d, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_link));
            break;
        case IrOpKind::J: {
            // After advance_pc, cpu.pc is the delay-slot address (= old_pc+4).
            mov(rax, pc_ptr_);
            mov(rax, qword[rax]);
            mov(rcx, 0xFFFFFFFFF0000000ULL);
            and_(rax, rcx);
            mov(rcx, static_cast<uint64_t>(op.target) << 2);
            or_(rax, rcx);
            mov(JIT_ARG2q, rax);
            mov(JIT_ARG1d, 1);
            call_fn(reinterpret_cast<const void *>(&do_branch_addr));
            break;
        }
        case IrOpKind::Jal: {
            mov(rax, pc_ptr_);
            mov(rax, qword[rax]);
            mov(r12, rax);
            mov(rcx, 0xFFFFFFFFF0000000ULL);
            and_(rax, rcx);
            mov(rcx, static_cast<uint64_t>(op.target) << 2);
            or_(rax, rcx);
            mov(JIT_ARG2q, rax);
            mov(JIT_ARG1d, 1);
            call_fn(reinterpret_cast<const void *>(&do_branch_addr));
            mov(JIT_ARG1d, 31);
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
            movzx(JIT_ARG1d, al);
            if (op.kind == IrOpKind::Bne || op.kind == IrOpKind::Bnel)
                xor_(JIT_ARG1d, 1);
            if (op.kind == IrOpKind::Beql || op.kind == IrOpKind::Bnel) {
                mov(JIT_ARG2d, off);
                call_fn(reinterpret_cast<const void *>(&do_branch_likely_offset));
            } else {
                emit_branch_offset_inline(off);
            }
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
            movzx(JIT_ARG1d, al);
            if (op.kind == IrOpKind::Blezl || op.kind == IrOpKind::Bgtzl) {
                mov(JIT_ARG2d, off);
                call_fn(reinterpret_cast<const void *>(&do_branch_likely_offset));
            } else {
                emit_branch_offset_inline(off);
            }
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
            movzx(JIT_ARG1d, al);
            const bool likely = op.kind == IrOpKind::Bltzl ||
                                op.kind == IrOpKind::Bgezl;
            if (likely) {
                mov(JIT_ARG2d, off);
                call_fn(reinterpret_cast<const void *>(&do_branch_likely_offset));
            } else {
                emit_branch_offset_inline(off);
            }
            if (op.kind == IrOpKind::Bltzal || op.kind == IrOpKind::Bgezal) {
                mov(JIT_ARG1d, 31);
                call_fn(reinterpret_cast<const void *>(&do_link));
            }
            break;
        }
        default:
            break;
        }
    }

    void emit_mem(const IrOp &op) {
        mov(JIT_ARG1d, op.rt);
        mov(JIT_ARG2d, op.rs);
        mov(JIT_ARG3d, static_cast<int16_t>(op.imm));
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
            mov(rax, hi_ptr_);
            mov(rax, qword[rax]);
            rax_to_gpr(op.rd);
            break;
        case IrOpKind::Mflo:
            mov(rax, lo_ptr_);
            mov(rax, qword[rax]);
            rax_to_gpr(op.rd);
            break;
        case IrOpKind::Mthi:
            gpr_to_rax(op.rs);
            mov(rdx, hi_ptr_);
            mov(qword[rdx], rax);
            break;
        case IrOpKind::Mtlo:
            gpr_to_rax(op.rs);
            mov(rdx, lo_ptr_);
            mov(qword[rdx], rax);
            break;
        case IrOpKind::Mult:
            mov(JIT_ARG1d, op.rs);
            mov(JIT_ARG2d, op.rt);
            call_fn(reinterpret_cast<const void *>(&do_mult));
            break;
        case IrOpKind::Multu:
            mov(JIT_ARG1d, op.rs);
            mov(JIT_ARG2d, op.rt);
            call_fn(reinterpret_cast<const void *>(&do_multu));
            break;
        case IrOpKind::Div:
            mov(JIT_ARG1d, op.rs);
            mov(JIT_ARG2d, op.rt);
            call_fn(reinterpret_cast<const void *>(&do_div));
            break;
        case IrOpKind::Divu:
            mov(JIT_ARG1d, op.rs);
            mov(JIT_ARG2d, op.rt);
            call_fn(reinterpret_cast<const void *>(&do_divu));
            break;
        case IrOpKind::Mfc0:
            mov(JIT_ARG1d, op.rt);
            mov(JIT_ARG2d, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_mfc0));
            break;
        case IrOpKind::Mtc0:
            mov(JIT_ARG1d, op.rt);
            mov(JIT_ARG2d, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_mtc0));
            break;
        case IrOpKind::Dmfc0:
            mov(JIT_ARG1d, op.rt);
            mov(JIT_ARG2d, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_dmfc0));
            break;
        case IrOpKind::Dmtc0:
            mov(JIT_ARG1d, op.rt);
            mov(JIT_ARG2d, op.rd);
            call_fn(reinterpret_cast<const void *>(&do_dmtc0));
            break;
        }
    }
};

} // namespace

BlockFn emit_block(const IrBlock &block, CodeCache &cache) {
    // Most blocks are small; 16 KiB is plenty for helper-call style emit.
    constexpr size_t kBufSize = 16 * 1024;
    uint8_t *buf = cache.alloc_exec(kBufSize);
    BlockEmitter emitter(buf, kBufSize);
    BlockFn fn = emitter.emit(block);
    // Reclaim unused tail of this bump allocation for the next block.
    cache.shrink_last_alloc(kBufSize, emitter.getSize());
    return fn;
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

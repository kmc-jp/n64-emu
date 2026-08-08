#include "rcp/jit/code_cache.h"
#include "rcp/jit/helpers.h"
#include "rcp/jit/ir.h"
#include "rcp/jit/jit.h"
#include "rcp/rsp.h"
#include <xbyak/xbyak.h>
#include <cstddef>
#include <cstdint>

namespace N64 {
namespace Rsp {
namespace Jit {

namespace {

using namespace Xbyak;
using namespace Xbyak::util;

#ifdef _WIN32
static constexpr size_t kAbiStackAdjust = 0x20;
#define JIT_ARG1d ecx
#define JIT_ARG2d edx
#else
static constexpr size_t kAbiStackAdjust = 0;
#define JIT_ARG1d edi
#define JIT_ARG2d esi
#endif

class BlockEmitter : public CodeGenerator {
  public:
    BlockEmitter(uint8_t *buf, size_t size) : CodeGenerator(size, buf) {
        Rsp &r = g_rsp();
        gpr_base_ = reinterpret_cast<uintptr_t>(r.gpr_data());
        pc_ptr_ = reinterpret_cast<uintptr_t>(r.pc_ptr());
        next_pc_ptr_ = reinterpret_cast<uintptr_t>(r.next_pc_ptr());
        delay_ptr_ = reinterpret_cast<uintptr_t>(r.delay_slot_ptr());
        status_ptr_ = reinterpret_cast<uintptr_t>(r.status_raw_ptr());
    }

    size_t emit(const IrBlock &block) {
        Label done;
        push(rbx);
        if (kAbiStackAdjust)
            sub(rsp, kAbiStackAdjust);
        xor_(ebx, ebx);

        auto advance_pc_n = [&](int n) {
            if (n < 1)
                return;
            mov(rax, next_pc_ptr_);
            movzx(ecx, word[rax]);
            if (n > 1) {
                add(ecx, 4 * (n - 1));
                and_(ecx, 0xffc);
            }
            mov(rdx, pc_ptr_);
            mov(word[rdx], cx);
            add(ecx, 4);
            and_(ecx, 0xffc);
            mov(word[rax], cx);
            mov(rax, delay_ptr_);
            mov(byte[rax], 0);
        };

        for (size_t idx = 0; idx < block.ops.size();) {
            Label after_sstep;
            const IrOp &op = block.ops[idx];

            mov(rax, status_ptr_);
            test(dword[rax], 1);
            jnz(done, T_NEAR);

            if (op.kind == IrOpKind::Exec) {
                mov(JIT_ARG1d, op.inst);
                mov(rax, reinterpret_cast<uintptr_t>(&exec_one));
                call(rax);
                test(eax, eax);
                jz(done, T_NEAR);
                inc(ebx);
                cmp(eax, 2);
                je(done, T_NEAR);
                ++idx;
                continue;
            }

            // Coalesce consecutive vector ops (compute / LWC2 / SWC2).
            if (op.kind == IrOpKind::VuCompute || op.kind == IrOpKind::Lwc2 ||
                op.kind == IrOpKind::Swc2) {
                size_t end = idx;
                while (end < block.ops.size()) {
                    const auto k = block.ops[end].kind;
                    if (k != IrOpKind::VuCompute && k != IrOpKind::Lwc2 &&
                        k != IrOpKind::Swc2)
                        break;
                    ++end;
                }
                const int n = static_cast<int>(end - idx);
                advance_pc_n(n);

                Label skip_data, data;
                jmp(skip_data, T_NEAR);
                L(data);
                for (size_t k = idx; k < end; ++k)
                    dd(block.ops[k].inst);
                L(skip_data);
#ifdef _WIN32
                lea(rcx, ptr[rip + data]);
                mov(edx, n);
#else
                lea(rdi, ptr[rip + data]);
                mov(esi, n);
#endif
                call_fn(reinterpret_cast<const void *>(&vu_run_vector_ops));
                add(ebx, n);

                mov(rax, status_ptr_);
                test(dword[rax], 1u << 5);
                jz(after_sstep);
                or_(dword[rax], 1);
                L(after_sstep);
                idx = end;
                continue;
            }

            advance_pc_n(1);
            emit_body(op);
            inc(ebx);
            ++idx;

            if (op.kind == IrOpKind::Break)
                jmp(done, T_NEAR);

            mov(rax, status_ptr_);
            test(dword[rax], 1u << 5);
            jz(after_sstep);
            or_(dword[rax], 1);
            L(after_sstep);
        }

        L(done);
        mov(eax, ebx);
        if (kAbiStackAdjust)
            add(rsp, kAbiStackAdjust);
        pop(rbx);
        ret();
        return getSize();
    }

  private:
    uintptr_t gpr_base_{};
    uintptr_t pc_ptr_{};
    uintptr_t next_pc_ptr_{};
    uintptr_t delay_ptr_{};
    uintptr_t status_ptr_{};

    void gpr_to_eax(uint8_t reg) {
        if (reg == 0) {
            xor_(eax, eax);
            return;
        }
        mov(rax, gpr_base_ + static_cast<uintptr_t>(reg) * 4);
        mov(eax, dword[rax]);
    }

    void eax_to_gpr(uint8_t reg) {
        if (reg == 0)
            return;
        mov(rdx, gpr_base_ + static_cast<uintptr_t>(reg) * 4);
        mov(dword[rdx], eax);
    }

    void call_fn(const void *fn) {
        mov(rax, reinterpret_cast<uintptr_t>(fn));
        call(rax);
    }

    void set_delay_target(uint16_t t) {
        mov(rax, delay_ptr_);
        mov(byte[rax], 1);
        mov(rax, next_pc_ptr_);
        mov(word[rax], static_cast<uint16_t>(t & 0xffc));
    }

    void emit_body(const IrOp &op) {
        switch (op.kind) {
        case IrOpKind::Addu: {
            gpr_to_eax(op.rs);
            mov(ecx, eax);
            gpr_to_eax(op.rt);
            add(ecx, eax);
            mov(eax, ecx);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Subu: {
            gpr_to_eax(op.rs);
            mov(ecx, eax);
            gpr_to_eax(op.rt);
            sub(ecx, eax);
            mov(eax, ecx);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::And: {
            gpr_to_eax(op.rs);
            mov(ecx, eax);
            gpr_to_eax(op.rt);
            and_(ecx, eax);
            mov(eax, ecx);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Or: {
            gpr_to_eax(op.rs);
            mov(ecx, eax);
            gpr_to_eax(op.rt);
            or_(ecx, eax);
            mov(eax, ecx);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Xor: {
            gpr_to_eax(op.rs);
            mov(ecx, eax);
            gpr_to_eax(op.rt);
            xor_(ecx, eax);
            mov(eax, ecx);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Nor: {
            gpr_to_eax(op.rs);
            mov(ecx, eax);
            gpr_to_eax(op.rt);
            or_(ecx, eax);
            not_(ecx);
            mov(eax, ecx);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Slt: {
            gpr_to_eax(op.rs);
            mov(ecx, eax);
            gpr_to_eax(op.rt);
            cmp(ecx, eax);
            setl(al);
            movzx(eax, al);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Sltu: {
            gpr_to_eax(op.rs);
            mov(ecx, eax);
            gpr_to_eax(op.rt);
            cmp(ecx, eax);
            setb(al);
            movzx(eax, al);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Sll: {
            gpr_to_eax(op.rt);
            if (op.sa & 31)
                shl(eax, op.sa & 31);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Srl: {
            gpr_to_eax(op.rt);
            if (op.sa & 31)
                shr(eax, op.sa & 31);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Sra: {
            gpr_to_eax(op.rt);
            if (op.sa & 31)
                sar(eax, op.sa & 31);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Sllv:
        case IrOpKind::Srlv:
        case IrOpKind::Srav: {
            gpr_to_eax(op.rt);
            mov(edx, eax);
            gpr_to_eax(op.rs);
            and_(eax, 31);
            mov(ecx, eax);
            mov(eax, edx);
            if (op.kind == IrOpKind::Sllv)
                shl(eax, cl);
            else if (op.kind == IrOpKind::Srlv)
                shr(eax, cl);
            else
                sar(eax, cl);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Addiu: {
            gpr_to_eax(op.rs);
            add(eax, static_cast<uint32_t>(static_cast<int16_t>(op.imm)));
            eax_to_gpr(op.rt);
            break;
        }
        case IrOpKind::Andi: {
            gpr_to_eax(op.rs);
            and_(eax, op.imm);
            eax_to_gpr(op.rt);
            break;
        }
        case IrOpKind::Ori: {
            gpr_to_eax(op.rs);
            or_(eax, op.imm);
            eax_to_gpr(op.rt);
            break;
        }
        case IrOpKind::Xori: {
            gpr_to_eax(op.rs);
            xor_(eax, op.imm);
            eax_to_gpr(op.rt);
            break;
        }
        case IrOpKind::Lui: {
            mov(eax, static_cast<uint32_t>(op.imm) << 16);
            eax_to_gpr(op.rt);
            break;
        }
        case IrOpKind::Slti: {
            gpr_to_eax(op.rs);
            cmp(eax, static_cast<uint32_t>(static_cast<int16_t>(op.imm)));
            setl(al);
            movzx(eax, al);
            eax_to_gpr(op.rt);
            break;
        }
        case IrOpKind::Sltiu: {
            gpr_to_eax(op.rs);
            cmp(eax, static_cast<uint32_t>(static_cast<int16_t>(op.imm)));
            setb(al);
            movzx(eax, al);
            eax_to_gpr(op.rt);
            break;
        }
        case IrOpKind::Beq:
        case IrOpKind::Bne: {
            mov(rax, delay_ptr_);
            mov(byte[rax], 1);
            gpr_to_eax(op.rs);
            mov(ecx, eax);
            gpr_to_eax(op.rt);
            Label skip;
            if (op.kind == IrOpKind::Beq) {
                cmp(ecx, eax);
                jne(skip, T_NEAR);
            } else {
                cmp(ecx, eax);
                je(skip, T_NEAR);
            }
            mov(rax, next_pc_ptr_);
            mov(word[rax], op.target);
            L(skip);
            break;
        }
        case IrOpKind::Blez:
        case IrOpKind::Bgtz:
        case IrOpKind::Bltz:
        case IrOpKind::Bgez:
        case IrOpKind::Bltzal:
        case IrOpKind::Bgezal: {
            mov(rax, delay_ptr_);
            mov(byte[rax], 1);
            gpr_to_eax(op.rs);
            Label skip;
            if (op.kind == IrOpKind::Blez) {
                test(eax, eax);
                jg(skip, T_NEAR);
            } else if (op.kind == IrOpKind::Bgtz) {
                test(eax, eax);
                jle(skip, T_NEAR);
            } else if (op.kind == IrOpKind::Bltz ||
                       op.kind == IrOpKind::Bltzal) {
                test(eax, eax);
                jge(skip, T_NEAR);
            } else {
                test(eax, eax);
                jl(skip, T_NEAR);
            }
            mov(rax, next_pc_ptr_);
            mov(word[rax], op.target);
            L(skip);
            if (op.kind == IrOpKind::Bltzal || op.kind == IrOpKind::Bgezal) {
                mov(eax, op.imm);
                eax_to_gpr(31);
            }
            break;
        }
        case IrOpKind::J:
            set_delay_target(op.target);
            break;
        case IrOpKind::Jal:
            set_delay_target(op.target);
            mov(eax, op.imm);
            eax_to_gpr(31);
            break;
        case IrOpKind::Jr: {
            mov(rax, delay_ptr_);
            mov(byte[rax], 1);
            gpr_to_eax(op.rs);
            and_(eax, 0xffc);
            mov(rdx, next_pc_ptr_);
            mov(word[rdx], ax);
            break;
        }
        case IrOpKind::Jalr: {
            mov(rax, delay_ptr_);
            mov(byte[rax], 1);
            gpr_to_eax(op.rs);
            and_(eax, 0xffc);
            mov(rdx, next_pc_ptr_);
            mov(word[rdx], ax);
            mov(eax, op.target);
            eax_to_gpr(op.rd);
            break;
        }
        case IrOpKind::Break:
            call_fn(reinterpret_cast<const void *>(&do_break));
            break;
        case IrOpKind::VuCompute: {
            mov(JIT_ARG1d, op.inst);
            call_fn(reinterpret_cast<const void *>(&vu_compute));
            break;
        }
        case IrOpKind::Lwc2:
        case IrOpKind::Swc2: {
            mov(JIT_ARG1d, op.inst);
            const void *fn = reinterpret_cast<const void *>(&vu_lwc2);
            if (op.kind == IrOpKind::Swc2)
                fn = reinterpret_cast<const void *>(&vu_swc2);
            call_fn(fn);
            break;
        }
        case IrOpKind::Lb:
        case IrOpKind::Lbu:
        case IrOpKind::Lh:
        case IrOpKind::Lhu:
        case IrOpKind::Lw: {
            gpr_to_eax(op.rs);
            add(eax, static_cast<uint32_t>(static_cast<int16_t>(op.imm)));
            mov(JIT_ARG1d, eax);
            const void *fn = reinterpret_cast<const void *>(&mem_lw);
            if (op.kind == IrOpKind::Lb)
                fn = reinterpret_cast<const void *>(&mem_lb);
            else if (op.kind == IrOpKind::Lbu)
                fn = reinterpret_cast<const void *>(&mem_lbu);
            else if (op.kind == IrOpKind::Lh)
                fn = reinterpret_cast<const void *>(&mem_lh);
            else if (op.kind == IrOpKind::Lhu)
                fn = reinterpret_cast<const void *>(&mem_lhu);
            call_fn(fn);
            eax_to_gpr(op.rt);
            break;
        }
        case IrOpKind::Sb:
        case IrOpKind::Sh:
        case IrOpKind::Sw: {
            gpr_to_eax(op.rs);
            add(eax, static_cast<uint32_t>(static_cast<int16_t>(op.imm)));
            mov(JIT_ARG1d, eax);
            gpr_to_eax(op.rt);
            mov(JIT_ARG2d, eax);
            const void *fn = reinterpret_cast<const void *>(&mem_sw);
            if (op.kind == IrOpKind::Sb)
                fn = reinterpret_cast<const void *>(&mem_sb);
            else if (op.kind == IrOpKind::Sh)
                fn = reinterpret_cast<const void *>(&mem_sh);
            call_fn(fn);
            break;
        }
        default:
            break;
        }
    }
};

} // namespace

BlockFn emit_block(const IrBlock &block, CodeCache &cache) {
    constexpr size_t kReserve = 16384;
    uint8_t *mem = cache.alloc_exec(kReserve);
    BlockEmitter emitter(mem, kReserve);
    const size_t used = emitter.emit(block);
    cache.shrink_last_alloc(kReserve, used);
    return reinterpret_cast<BlockFn>(mem);
}

} // namespace Jit
} // namespace Rsp
} // namespace N64

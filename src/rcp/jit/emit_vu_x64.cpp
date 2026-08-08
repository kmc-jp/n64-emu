#include "rcp/jit/emit_vu_x64.h"

namespace N64 {
namespace Rsp {
namespace Jit {
namespace {

using namespace Xbyak;
using namespace Xbyak::util;

// Live ACC: xmm0=L, xmm1=M, xmm2=H. xmm7=zero for the streak.
// Temps: xmm3=vs, xmm4=vt, xmm5/xmm6/xmm8/xmm9/xmm10 scratch.

void load_vreg(CodeGenerator &g, const Xmm &dst, uint8_t reg) {
    g.movdqa(dst, g.xword[g.r13 + static_cast<uintptr_t>(reg) * 16u]);
}

void store_vreg(CodeGenerator &g, const Xmm &src, uint8_t reg) {
    g.movdqa(g.xword[g.r13 + static_cast<uintptr_t>(reg) * 16u], src);
}

void emit_shuffle_vt(CodeGenerator &g, const Xmm &dst, uint8_t vt,
                     unsigned e) {
    if (e <= 1) {
        load_vreg(g, dst, vt);
        return;
    }
    if (e == 2 || e == 3) {
        load_vreg(g, dst, vt);
        const int imm = (e == 2) ? 0xA0 : 0xF5; // SHUFFLE(2,2,0,0)/(3,3,1,1)
        g.pshuflw(dst, dst, imm);
        g.pshufhw(dst, dst, imm);
        return;
    }
    if (e >= 4 && e <= 7) {
        g.pxor(dst, dst);
        g.movzx(g.ecx, g.word[g.r13 + static_cast<uintptr_t>(vt) * 16u +
                                   static_cast<uintptr_t>(e - 4u) * 2u]);
        g.pinsrw(dst, g.ecx, 0);
        g.movzx(g.ecx, g.word[g.r13 + static_cast<uintptr_t>(vt) * 16u +
                                   static_cast<uintptr_t>(e) * 2u]);
        g.pinsrw(dst, g.ecx, 1);
        g.pshuflw(dst, dst, 0x50);
        g.pshufd(dst, dst, 0x50);
        return;
    }
    g.pxor(dst, dst);
    g.movzx(g.ecx, g.word[g.r13 + static_cast<uintptr_t>(vt) * 16u +
                               static_cast<uintptr_t>(e - 8u) * 2u]);
    g.pinsrw(dst, g.ecx, 0);
    g.punpcklwd(dst, dst);
    g.pshufd(dst, dst, 0x00);
}

// carry_ffff in `ov`: 0xFFFF lanes where unsigned add overflowed.
void emit_acc_add_u16(CodeGenerator &g, const Xmm &dst, const Xmm &src,
                      const Xmm &zero, const Xmm &ov, const Xmm &sat) {
    g.movdqa(sat, dst);
    g.paddusw(sat, src);
    g.paddw(dst, src);
    g.movdqa(ov, dst);
    g.pcmpeqw(ov, sat);
    g.pcmpeqw(ov, zero);
}

void emit_sclamp_md_hi(CodeGenerator &g, const Xmm &out, const Xmm &md,
                       const Xmm &hi, const Xmm &t0, const Xmm &t1) {
    g.movdqa(t0, md);
    g.movdqa(t1, md);
    g.punpcklwd(t0, hi);
    g.punpckhwd(t1, hi);
    g.movdqa(out, t0);
    g.packssdw(out, t1);
}

// hi_neg/ok/t are scratch. Result in `out`.
void emit_uclamp_lo_md_hi_fixed(CodeGenerator &g, const Xmm &out, const Xmm &lo,
                                const Xmm &md, const Xmm &hi, const Xmm &zero,
                                const Xmm &hi_neg, const Xmm &ok,
                                const Xmm &t) {
    g.movdqa(hi_neg, hi);
    g.psraw(hi_neg, 15);
    g.movdqa(t, md);
    g.psraw(t, 15); // md_neg
    g.movdqa(ok, hi_neg);
    g.pcmpeqw(ok, hi); // hi_ok
    g.pcmpeqw(t, hi_neg); // md_ok (t was md_neg)
    g.pand(ok, t);
    g.movdqa(t, hi_neg);
    g.pcmpeqw(t, zero); // clamped
    g.movdqa(out, ok);
    g.pandn(out, t); // repl = (~ok) & clamped
    g.movdqa(t, ok);
    g.pand(t, lo); // keep
    g.por(out, t);
}

void emit_vmadh(CodeGenerator &g, bool accumulate) {
    // vs=xmm3, vt=xmm4, zero=xmm7, acc=xmm0/1/2, out→xmm5
    // temps xmm6,xmm8,xmm9
    if (!accumulate) {
        g.pxor(g.xmm0, g.xmm0);
        g.movdqa(g.xmm1, g.xmm3);
        g.pmullw(g.xmm1, g.xmm4);
        g.movdqa(g.xmm2, g.xmm3);
        g.pmulhw(g.xmm2, g.xmm4);
        emit_sclamp_md_hi(g, g.xmm5, g.xmm1, g.xmm2, g.xmm6, g.xmm8);
        return;
    }
    g.movdqa(g.xmm5, g.xmm3);
    g.pmullw(g.xmm5, g.xmm4); // lo
    g.movdqa(g.xmm6, g.xmm3);
    g.pmulhw(g.xmm6, g.xmm4); // hi
    emit_acc_add_u16(g, g.xmm1, g.xmm5, g.xmm7, g.xmm8, g.xmm9); // ov in xmm8
    g.psubw(g.xmm6, g.xmm8);
    g.paddw(g.xmm2, g.xmm6);
    emit_sclamp_md_hi(g, g.xmm5, g.xmm1, g.xmm2, g.xmm6, g.xmm8);
}

void emit_vmadn(CodeGenerator &g, bool accumulate) {
    // lo = mullo; hi = mulhiu; hi -= vs & (vt>>15)
    g.movdqa(g.xmm5, g.xmm3);
    g.pmullw(g.xmm5, g.xmm4); // lo
    g.movdqa(g.xmm6, g.xmm3);
    g.pmulhuw(g.xmm6, g.xmm4); // hi
    g.movdqa(g.xmm8, g.xmm4);
    g.psraw(g.xmm8, 15);
    g.pand(g.xmm8, g.xmm3);
    g.psubw(g.xmm6, g.xmm8);
    if (!accumulate) {
        g.movdqa(g.xmm0, g.xmm5);
        g.movdqa(g.xmm1, g.xmm6);
        g.movdqa(g.xmm2, g.xmm6);
        g.psraw(g.xmm2, 15);
        g.movdqa(g.xmm5, g.xmm0); // out = lo
        return;
    }
    emit_acc_add_u16(g, g.xmm0, g.xmm5, g.xmm7, g.xmm8, g.xmm9);
    g.psubw(g.xmm6, g.xmm8);
    emit_acc_add_u16(g, g.xmm1, g.xmm6, g.xmm7, g.xmm8, g.xmm9);
    g.movdqa(g.xmm5, g.xmm6);
    g.psraw(g.xmm5, 15);
    g.paddw(g.xmm2, g.xmm5);
    g.psubw(g.xmm2, g.xmm8);
    emit_uclamp_lo_md_hi_fixed(g, g.xmm5, g.xmm0, g.xmm1, g.xmm2, g.xmm7,
                               g.xmm6, g.xmm8, g.xmm9);
}

void emit_vmadm(CodeGenerator &g, bool accumulate) {
    g.movdqa(g.xmm5, g.xmm3);
    g.pmullw(g.xmm5, g.xmm4);
    g.movdqa(g.xmm6, g.xmm3);
    g.pmulhuw(g.xmm6, g.xmm4);
    g.movdqa(g.xmm8, g.xmm3);
    g.psraw(g.xmm8, 15);
    g.pand(g.xmm8, g.xmm4);
    g.psubw(g.xmm6, g.xmm8);
    if (!accumulate) {
        g.movdqa(g.xmm0, g.xmm5);
        g.movdqa(g.xmm1, g.xmm6);
        g.movdqa(g.xmm2, g.xmm6);
        g.psraw(g.xmm2, 15);
        g.movdqa(g.xmm5, g.xmm1); // out = hi/md
        return;
    }
    emit_acc_add_u16(g, g.xmm0, g.xmm5, g.xmm7, g.xmm8, g.xmm9);
    g.psubw(g.xmm6, g.xmm8);
    emit_acc_add_u16(g, g.xmm1, g.xmm6, g.xmm7, g.xmm8, g.xmm9);
    g.movdqa(g.xmm5, g.xmm6);
    g.psraw(g.xmm5, 15);
    g.paddw(g.xmm2, g.xmm5);
    g.psubw(g.xmm2, g.xmm8);
    emit_sclamp_md_hi(g, g.xmm5, g.xmm1, g.xmm2, g.xmm6, g.xmm8);
}

void emit_vmadl(CodeGenerator &g, bool accumulate) {
    g.movdqa(g.xmm5, g.xmm3);
    g.pmulhuw(g.xmm5, g.xmm4); // hi product bits → lo slice
    if (!accumulate) {
        g.movdqa(g.xmm0, g.xmm5);
        g.pxor(g.xmm1, g.xmm1);
        g.pxor(g.xmm2, g.xmm2);
        return;
    }
    emit_acc_add_u16(g, g.xmm0, g.xmm5, g.xmm7, g.xmm8, g.xmm9);
    g.movdqa(g.xmm6, g.xmm7);
    g.psubw(g.xmm6, g.xmm8); // carry = 0 - ov = -ov as u16... C uses sub(zero,ov)
    emit_acc_add_u16(g, g.xmm1, g.xmm6, g.xmm7, g.xmm8, g.xmm9);
    g.psubw(g.xmm2, g.xmm8);
    emit_uclamp_lo_md_hi_fixed(g, g.xmm5, g.xmm0, g.xmm1, g.xmm2, g.xmm7,
                               g.xmm6, g.xmm8, g.xmm9);
}

void emit_vmulf(CodeGenerator &g) {
    // Matches op_vmulf in vu_sse.cpp
    g.movdqa(g.xmm5, g.xmm3);
    g.pmullw(g.xmm5, g.xmm4); // lo
    g.pcmpeqw(g.xmm8, g.xmm8); // round = all 1s
    g.movdqa(g.xmm9, g.xmm5);
    g.psrlw(g.xmm9, 15); // sign1
    g.paddw(g.xmm5, g.xmm5); // lo <<= 1 (add to self)
    g.psllw(g.xmm8, 15); // round = 0x8000
    g.movdqa(g.xmm6, g.xmm3);
    g.pmulhw(g.xmm6, g.xmm4); // hi
    g.movdqa(g.xmm10, g.xmm5);
    g.psrlw(g.xmm10, 15); // sign2
    g.movdqa(g.xmm0, g.xmm8);
    g.paddw(g.xmm0, g.xmm5); // acc_l
    g.paddw(g.xmm9, g.xmm10); // sign1+sign2
    g.psllw(g.xmm6, 1);
    g.movdqa(g.xmm8, g.xmm3);
    g.pcmpeqw(g.xmm8, g.xmm4); // eq
    g.movdqa(g.xmm1, g.xmm6);
    g.paddw(g.xmm1, g.xmm9); // acc_m
    g.movdqa(g.xmm2, g.xmm1);
    g.psraw(g.xmm2, 15); // neg
    g.movdqa(g.xmm5, g.xmm8);
    g.pand(g.xmm5, g.xmm2); // eq_and_neg
    g.pandn(g.xmm8, g.xmm2); // acc_h = ~eq & neg
    g.movdqa(g.xmm2, g.xmm8);
    g.movdqa(g.xmm8, g.xmm1);
    g.paddw(g.xmm8, g.xmm5); // out
    g.movdqa(g.xmm5, g.xmm8);
}

void emit_vmacf(CodeGenerator &g) {
    g.movdqa(g.xmm5, g.xmm3);
    g.pmullw(g.xmm5, g.xmm4); // lo
    g.movdqa(g.xmm6, g.xmm3);
    g.pmulhw(g.xmm6, g.xmm4); // hi
    g.movdqa(g.xmm8, g.xmm6);
    g.psllw(g.xmm8, 1); // md
    g.movdqa(g.xmm9, g.xmm5);
    g.psrlw(g.xmm9, 15); // carry from lo
    g.psraw(g.xmm6, 15); // hi sign-extend-ish
    g.por(g.xmm8, g.xmm9); // md |= carry
    g.psllw(g.xmm5, 1); // lo <<= 1
    emit_acc_add_u16(g, g.xmm0, g.xmm5, g.xmm7, g.xmm9, g.xmm10);
    g.psubw(g.xmm8, g.xmm9); // md -= ov
    g.movdqa(g.xmm5, g.xmm8);
    g.pcmpeqw(g.xmm5, g.xmm7);
    g.pand(g.xmm5, g.xmm9); // carry = (md==0) & ov
    g.psubw(g.xmm6, g.xmm5);
    emit_acc_add_u16(g, g.xmm1, g.xmm8, g.xmm7, g.xmm9, g.xmm10);
    g.paddw(g.xmm2, g.xmm6);
    g.psubw(g.xmm2, g.xmm9);
    emit_sclamp_md_hi(g, g.xmm5, g.xmm1, g.xmm2, g.xmm6, g.xmm8);
}

void emit_one(CodeGenerator &g, const IrOp &op) {
    const unsigned funct = op.imm & 0x3Fu;
    const unsigned e = op.sa & 0xFu;
    load_vreg(g, g.xmm3, op.rs);
    emit_shuffle_vt(g, g.xmm4, op.rt, e);

    switch (funct) {
    case 0x00:
        emit_vmulf(g);
        break;
    case 0x04:
        emit_vmadl(g, false);
        break;
    case 0x05:
        emit_vmadm(g, false);
        break;
    case 0x06:
        emit_vmadn(g, false);
        break;
    case 0x07:
        emit_vmadh(g, false);
        break;
    case 0x08:
        emit_vmacf(g);
        break;
    case 0x0C:
        emit_vmadl(g, true);
        break;
    case 0x0D:
        emit_vmadm(g, true);
        break;
    case 0x0E:
        emit_vmadn(g, true);
        break;
    case 0x0F:
        emit_vmadh(g, true);
        break;
    default:
        break;
    }
    store_vreg(g, g.xmm5, op.rd);
}

} // namespace

bool vu_funct_inlinable(uint32_t funct) {
    switch (funct & 0x3Fu) {
    case 0x00:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F:
        return true;
    default:
        return false;
    }
}

void emit_vu_inline_streak(CodeGenerator &g, uintptr_t vpr_base, uintptr_t acc_l,
                           uintptr_t acc_m, uintptr_t acc_h, const IrOp *ops,
                           size_t count) {
    if (count == 0)
        return;

    // r13 = vpr base for the streak (callee-saved; no calls here).
    g.push(g.r13);
    g.mov(g.r13, vpr_base);

    g.mov(g.rax, acc_l);
    g.movdqa(g.xmm0, g.xword[g.rax]);
    g.mov(g.rax, acc_m);
    g.movdqa(g.xmm1, g.xword[g.rax]);
    g.mov(g.rax, acc_h);
    g.movdqa(g.xmm2, g.xword[g.rax]);
    g.pxor(g.xmm7, g.xmm7);

    for (size_t i = 0; i < count; ++i)
        emit_one(g, ops[i]);

    g.mov(g.rax, acc_l);
    g.movdqa(g.xword[g.rax], g.xmm0);
    g.mov(g.rax, acc_m);
    g.movdqa(g.xword[g.rax], g.xmm1);
    g.mov(g.rax, acc_h);
    g.movdqa(g.xword[g.rax], g.xmm2);

    g.pop(g.r13);
}

} // namespace Jit
} // namespace Rsp
} // namespace N64

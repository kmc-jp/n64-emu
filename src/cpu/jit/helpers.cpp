#include "cpu/jit/helpers.h"
#include "cpu/cpu.h"
#include "cpu/jit/jit.h"
#include "memory/bus.h"
#include "mmu/mmu.h"
#include "mmu/tlb.h"
#include "n64_system/interrupt.h"

namespace N64 {
namespace Cpu {
namespace Jit {

namespace {
// Not thread_local: the JIT embeds absolute addresses of this object.
ExecState g_exec{};
}

ExecState &exec_state() { return g_exec; }
ExecState *exec_state_ptr() { return &g_exec; }

void advance_pc() { g_cpu().advance_pc_no_fetch(); }

bool block_entry_checks() {
    auto &cpu = g_cpu();
    if (cpu.should_service_interrupt()) {
        cpu.handle_exception(ExceptionCode::INTERRUPT, 0, false);
        return true;
    }
    return false;
}

void add_count(int n) { g_cpu().add_count(static_cast<uint32_t>(n)); }

void do_branch_addr(bool cond, uint64_t target) {
    Cpu::branch_addr64(g_cpu(), cond, target);
}

void do_branch_likely_addr(bool cond, uint64_t target) {
    // Record annul so the emitter can skip the delay-slot op compiled after
    // this branch in the same block (interpreter skips via set_pc64).
    exec_state().annul_delay_slot = !cond;
    Cpu::branch_likely_addr64(g_cpu(), cond, target);
}

void do_branch_offset(bool cond, int16_t offset) {
    instruction_t inst{};
    inst.i_type.imm = static_cast<uint16_t>(offset);
    Cpu::branch_offset16(g_cpu(), cond, inst);
}

void do_branch_likely_offset(bool cond, int16_t offset) {
    instruction_t inst{};
    inst.i_type.imm = static_cast<uint16_t>(offset);
    // Keep annul in sync with the same condition the interpreter uses.
    exec_state().annul_delay_slot = !cond;
    Cpu::branch_likely_offset16(g_cpu(), cond, inst);
}

void do_link(uint8_t reg) { Cpu::link(g_cpu(), reg); }

uint64_t gpr_get(uint8_t n) { return g_cpu().gpr.read(n); }

void gpr_set(uint8_t n, uint64_t v) { g_cpu().gpr.write(n, v); }

uint64_t get_hi() { return g_cpu().hi; }
uint64_t get_lo() { return g_cpu().lo; }
void set_hi(uint64_t v) { g_cpu().hi = v; }
void set_lo(uint64_t v) { g_cpu().lo = v; }

uint64_t get_pc() { return g_cpu().get_pc64(); }

void do_mult(uint8_t rs, uint8_t rt) {
    const int32_t s = static_cast<int32_t>(g_cpu().gpr.read(rs));
    const int32_t t = static_cast<int32_t>(g_cpu().gpr.read(rt));
    const int64_t res = static_cast<int64_t>(s) * static_cast<int64_t>(t);
    g_cpu().lo = static_cast<int64_t>(static_cast<int32_t>(res));
    g_cpu().hi = static_cast<int64_t>(static_cast<int32_t>(res >> 32));
}

void do_multu(uint8_t rs, uint8_t rt) {
    const uint64_t s = g_cpu().gpr.read(rs) & 0xFFFFFFFFu;
    const uint64_t t = g_cpu().gpr.read(rt) & 0xFFFFFFFFu;
    const uint64_t res = s * t;
    g_cpu().lo = static_cast<int64_t>(static_cast<int32_t>(res));
    g_cpu().hi = static_cast<int64_t>(static_cast<int32_t>(res >> 32));
}

void do_div(uint8_t rs, uint8_t rt) {
    const int64_t dividend = static_cast<int32_t>(g_cpu().gpr.read(rs));
    const int64_t divisor = static_cast<int32_t>(g_cpu().gpr.read(rt));
    if (divisor == 0) {
        g_cpu().hi = dividend;
        g_cpu().lo = dividend >= 0 ? static_cast<int64_t>(-1) : static_cast<int64_t>(1);
    } else {
        g_cpu().lo = static_cast<int32_t>(dividend / divisor);
        g_cpu().hi = static_cast<int32_t>(dividend % divisor);
    }
}

void do_divu(uint8_t rs, uint8_t rt) {
    const uint32_t dividend = static_cast<uint32_t>(g_cpu().gpr.read(rs));
    const uint32_t divisor = static_cast<uint32_t>(g_cpu().gpr.read(rt));
    if (divisor == 0) {
        g_cpu().hi = static_cast<int32_t>(dividend);
        g_cpu().lo = static_cast<int64_t>(-1);
    } else {
        g_cpu().lo = static_cast<int32_t>(dividend / divisor);
        g_cpu().hi = static_cast<int32_t>(dividend % divisor);
    }
}

namespace {

template <typename ReadFn, typename WriteGprFn>
void do_load(uint8_t rt, uint8_t base, int16_t offset, ReadFn read,
             WriteGprFn write_val) {
    auto &cpu = g_cpu();
    const uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr = Mmu::resolve_vaddr(static_cast<uint32_t>(vaddr));
    if (paddr.has_value()) {
        write_val(cpu, rt, read(paddr.value()));
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::LOAD), 0, true);
        exec_state().aborted = true;
    }
}

template <typename StoreFn>
void do_store(uint8_t rt, uint8_t base, int16_t offset, StoreFn store) {
    auto &cpu = g_cpu();
    const uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr =
        Mmu::resolve_vaddr(static_cast<uint32_t>(vaddr), Mmu::BusAccess::STORE);
    if (paddr.has_value()) {
        store(paddr.value(), cpu.gpr.read(rt));
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::STORE), 0, true);
        exec_state().aborted = true;
    }
}

} // namespace

void do_lb(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(rt, base, offset,
            [](uint32_t p) { return Memory::read_paddr8(p); },
            [](Cpu &c, uint8_t r, uint8_t v) {
                c.gpr.write(r, static_cast<int64_t>(static_cast<int8_t>(v)));
            });
}

void do_lbu(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(rt, base, offset,
            [](uint32_t p) { return Memory::read_paddr8(p); },
            [](Cpu &c, uint8_t r, uint8_t v) {
                c.gpr.write(r, static_cast<uint64_t>(v));
            });
}

void do_lh(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(rt, base, offset,
            [](uint32_t p) { return Memory::read_paddr16(p); },
            [](Cpu &c, uint8_t r, uint16_t v) {
                c.gpr.write(r, static_cast<int64_t>(static_cast<int16_t>(v)));
            });
}

void do_lhu(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(rt, base, offset,
            [](uint32_t p) { return Memory::read_paddr16(p); },
            [](Cpu &c, uint8_t r, uint16_t v) {
                c.gpr.write(r, static_cast<uint64_t>(v));
            });
}

void do_lw(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(rt, base, offset,
            [](uint32_t p) { return Memory::read_paddr32(p); },
            [](Cpu &c, uint8_t r, uint32_t v) {
                c.gpr.write(r, static_cast<int64_t>(static_cast<int32_t>(v)));
            });
}

void do_lwu(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(rt, base, offset,
            [](uint32_t p) { return Memory::read_paddr32(p); },
            [](Cpu &c, uint8_t r, uint32_t v) {
                c.gpr.write(r, static_cast<uint64_t>(v));
            });
}

void do_ld(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(rt, base, offset,
            [](uint32_t p) { return Memory::read_paddr64(p); },
            [](Cpu &c, uint8_t r, uint64_t v) { c.gpr.write(r, v); });
}

void do_sb(uint8_t rt, uint8_t base, int16_t offset) {
    do_store(rt, base, offset, [](uint32_t p, uint64_t v) {
        Memory::write_paddr8(p, static_cast<uint8_t>(v));
    });
}

void do_sh(uint8_t rt, uint8_t base, int16_t offset) {
    do_store(rt, base, offset, [](uint32_t p, uint64_t v) {
        Memory::write_paddr16(p, static_cast<uint16_t>(v));
    });
}

void do_sw(uint8_t rt, uint8_t base, int16_t offset) {
    do_store(rt, base, offset, [](uint32_t p, uint64_t v) {
        Memory::write_paddr32(p, static_cast<uint32_t>(v));
    });
}

void do_sd(uint8_t rt, uint8_t base, int16_t offset) {
    do_store(rt, base, offset,
             [](uint32_t p, uint64_t v) { Memory::write_paddr64(p, v); });
}

void do_mfc0(uint8_t rt, uint8_t rd) {
    // Match interpreter: 32-bit read, sign-extended.
    const uint32_t val =
        static_cast<uint32_t>(g_cpu().cop0.reg.read(rd));
    g_cpu().gpr.write(rt, static_cast<int64_t>(static_cast<int32_t>(val)));
}

void do_mtc0(uint8_t rt, uint8_t rd) {
    g_cpu().cop0.reg.write(rd, static_cast<uint32_t>(g_cpu().gpr.read(rt)));
}

void do_dmfc0(uint8_t rt, uint8_t rd) {
    g_cpu().gpr.write(rt, g_cpu().cop0.reg.read(rd));
}

void do_dmtc0(uint8_t rt, uint8_t rd) {
    g_cpu().cop0.reg.write(rd, g_cpu().gpr.read(rt));
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

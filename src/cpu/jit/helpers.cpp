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
thread_local ExecState g_exec{};
}

ExecState &exec_state() { return g_exec; }

void advance_pc() { g_cpu().advance_pc_no_fetch(); }

bool block_entry_checks() {
    auto &cpu = g_cpu();
    if (cpu.cop0.reg.count == (cpu.cop0.reg.compare << 1)) {
        cpu.cop0.reg.cause.ip7 = true;
        N64System::check_interrupt();
    }
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

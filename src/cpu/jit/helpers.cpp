#include "cpu/jit/helpers.h"
#include "cpu/cpu.h"
#include "cpu/instruction.h"
#include "cpu/jit/jit.h"
#include "memory/bus.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "mmu/mmu.h"
#include "mmu/soft_tlb.h"
#include "mmu/tlb.h"
#include "n64_system/interrupt.h"
#include "utils/byte_array.h"
#include <optional>
#include <span>

namespace N64 {
namespace Cpu {
namespace Jit {

namespace {
// Not thread_local: the JIT embeds absolute addresses of this object.
ExecState g_exec{};

uint8_t *rdram_data() { return g_memory().get_rdram().data(); }

bool paddr_in_rdram(uint32_t paddr, uint32_t access_size) {
    return paddr <= PHYS_RDRAM_MEM_END &&
           paddr + (access_size - 1) <= PHYS_RDRAM_MEM_END;
}

std::optional<uint32_t> soft_lookup(uint32_t vaddr, uint32_t access_size,
                                    bool is_store) {
    return Mmu::soft_tlb_lookup(vaddr, access_size, is_store);
}
} // namespace

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

void do_j(uint32_t target26) {
    auto &cpu = g_cpu();
    uint64_t target = static_cast<uint64_t>(target26) << 2;
    target |= ((cpu.get_pc64() - 4) & 0xFFFFFFFFF0000000ULL);
    Cpu::branch_addr64(cpu, true, target);
}

void do_jal(uint32_t target26) {
    auto &cpu = g_cpu();
    Cpu::link(cpu, RA);
    uint64_t target = static_cast<uint64_t>(target26) << 2;
    target |= ((cpu.get_pc64() - 4) & 0xFFFFFFFFF0000000ULL);
    Cpu::branch_addr64(cpu, true, target);
}

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

template <typename ReadRdram, typename ReadBus, typename WriteGprFn>
void do_load(uint8_t rt, uint8_t base, int16_t offset, uint32_t access_size,
             ReadRdram read_rdram, ReadBus read_bus, WriteGprFn write_val) {
    auto &cpu = g_cpu();
    const uint32_t va32 =
        static_cast<uint32_t>(cpu.gpr.read(base) + offset);
    if (auto cached = soft_lookup(va32, access_size, false)) {
        const uint32_t p = cached.value();
        if (paddr_in_rdram(p, access_size)) {
            write_val(cpu, rt, read_rdram(p));
            return;
        }
    }
    std::optional<uint32_t> paddr = Mmu::resolve_vaddr(va32);
    if (paddr.has_value()) {
        const uint32_t p = paddr.value();
        if (paddr_in_rdram(p, access_size)) {
            Mmu::soft_tlb_note_load(va32, p);
            write_val(cpu, rt, read_rdram(p));
        } else {
            write_val(cpu, rt, read_bus(p));
        }
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::LOAD), 0, true);
        exec_state().aborted = true;
    }
}

template <typename StoreRdram, typename StoreBus>
void do_store(uint8_t rt, uint8_t base, int16_t offset, uint32_t access_size,
              StoreRdram store_rdram, StoreBus store_bus) {
    auto &cpu = g_cpu();
    const uint32_t va32 =
        static_cast<uint32_t>(cpu.gpr.read(base) + offset);
    const uint64_t v = cpu.gpr.read(rt);
    if (auto cached = soft_lookup(va32, access_size, true)) {
        const uint32_t p = cached.value();
        if (paddr_in_rdram(p, access_size)) {
            store_rdram(p, v);
            return;
        }
    }
    std::optional<uint32_t> paddr =
        Mmu::resolve_vaddr(va32, Mmu::BusAccess::STORE);
    if (paddr.has_value()) {
        const uint32_t p = paddr.value();
        if (paddr_in_rdram(p, access_size)) {
            Mmu::soft_tlb_note_store(va32, p);
            store_rdram(p, v);
        } else {
            store_bus(p, v);
        }
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::STORE), 0, true);
        exec_state().aborted = true;
    }
}

} // namespace

void do_lb(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(
        rt, base, offset, 1,
        [](uint32_t p) {
            return Utils::read_from_byte_array8(
                std::span<const uint8_t>(rdram_data(), RDRAM_SIZE), p);
        },
        [](uint32_t p) { return Memory::read_paddr8(p); },
        [](Cpu &c, uint8_t r, uint8_t v) {
            c.gpr.write(r, static_cast<int64_t>(static_cast<int8_t>(v)));
        });
}

void do_lbu(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(
        rt, base, offset, 1,
        [](uint32_t p) {
            return Utils::read_from_byte_array8(
                std::span<const uint8_t>(rdram_data(), RDRAM_SIZE), p);
        },
        [](uint32_t p) { return Memory::read_paddr8(p); },
        [](Cpu &c, uint8_t r, uint8_t v) {
            c.gpr.write(r, static_cast<uint64_t>(v));
        });
}

void do_lh(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(
        rt, base, offset, 2,
        [](uint32_t p) {
            return Utils::read_from_byte_array16(
                std::span<const uint8_t>(rdram_data(), RDRAM_SIZE), p);
        },
        [](uint32_t p) { return Memory::read_paddr16(p); },
        [](Cpu &c, uint8_t r, uint16_t v) {
            c.gpr.write(r, static_cast<int64_t>(static_cast<int16_t>(v)));
        });
}

void do_lhu(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(
        rt, base, offset, 2,
        [](uint32_t p) {
            return Utils::read_from_byte_array16(
                std::span<const uint8_t>(rdram_data(), RDRAM_SIZE), p);
        },
        [](uint32_t p) { return Memory::read_paddr16(p); },
        [](Cpu &c, uint8_t r, uint16_t v) {
            c.gpr.write(r, static_cast<uint64_t>(v));
        });
}

void do_lw(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(
        rt, base, offset, 4,
        [](uint32_t p) {
            return Utils::read_from_byte_array32(
                std::span<const uint8_t>(rdram_data(), RDRAM_SIZE), p);
        },
        [](uint32_t p) { return Memory::read_paddr32(p); },
        [](Cpu &c, uint8_t r, uint32_t v) {
            c.gpr.write(r, static_cast<int64_t>(static_cast<int32_t>(v)));
        });
}

void do_lwu(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(
        rt, base, offset, 4,
        [](uint32_t p) {
            return Utils::read_from_byte_array32(
                std::span<const uint8_t>(rdram_data(), RDRAM_SIZE), p);
        },
        [](uint32_t p) { return Memory::read_paddr32(p); },
        [](Cpu &c, uint8_t r, uint32_t v) {
            c.gpr.write(r, static_cast<uint64_t>(v));
        });
}

void do_ld(uint8_t rt, uint8_t base, int16_t offset) {
    do_load(
        rt, base, offset, 8,
        [](uint32_t p) {
            return Utils::read_from_byte_array64(
                std::span<const uint8_t>(rdram_data(), RDRAM_SIZE), p);
        },
        [](uint32_t p) { return Memory::read_paddr64(p); },
        [](Cpu &c, uint8_t r, uint64_t v) { c.gpr.write(r, v); });
}

void do_lwl(uint8_t rt, uint8_t base, int16_t offset) {
    auto &cpu = g_cpu();
    const uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr =
        Mmu::resolve_vaddr(static_cast<uint32_t>(vaddr));
    if (paddr.has_value()) {
        const uint32_t shift = 8 * static_cast<uint32_t>((vaddr ^ 0) & 3);
        const uint32_t mask = 0xFFFFFFFFu << shift;
        const uint32_t aligned = paddr.value() & ~3u;
        const uint32_t data =
            paddr_in_rdram(aligned, 4)
                ? Utils::read_from_byte_array32(
                      std::span<const uint8_t>(rdram_data(), RDRAM_SIZE),
                      aligned)
                : Memory::read_paddr32(aligned);
        const uint32_t old = static_cast<uint32_t>(cpu.gpr.read(rt));
        const int32_t result =
            static_cast<int32_t>((old & ~mask) | (data << shift));
        cpu.gpr.write(rt, static_cast<int64_t>(result));
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::LOAD), 0, true);
        exec_state().aborted = true;
    }
}

void do_lwr(uint8_t rt, uint8_t base, int16_t offset) {
    auto &cpu = g_cpu();
    const uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr =
        Mmu::resolve_vaddr(static_cast<uint32_t>(vaddr));
    if (paddr.has_value()) {
        const uint32_t shift = 8 * static_cast<uint32_t>((vaddr ^ 3) & 3);
        const uint32_t mask = 0xFFFFFFFFu >> shift;
        const uint32_t aligned = paddr.value() & ~3u;
        const uint32_t data =
            paddr_in_rdram(aligned, 4)
                ? Utils::read_from_byte_array32(
                      std::span<const uint8_t>(rdram_data(), RDRAM_SIZE),
                      aligned)
                : Memory::read_paddr32(aligned);
        const uint32_t old = static_cast<uint32_t>(cpu.gpr.read(rt));
        const int32_t result =
            static_cast<int32_t>((old & ~mask) | (data >> shift));
        cpu.gpr.write(rt, static_cast<int64_t>(result));
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::LOAD), 0, true);
        exec_state().aborted = true;
    }
}

void do_sb(uint8_t rt, uint8_t base, int16_t offset) {
    do_store(
        rt, base, offset, 1,
        [](uint32_t p, uint64_t v) {
            Utils::write_to_byte_array8(
                std::span<uint8_t>(rdram_data(), RDRAM_SIZE), p,
                static_cast<uint8_t>(v));
        },
        [](uint32_t p, uint64_t v) {
            Memory::write_paddr8(p, static_cast<uint8_t>(v));
        });
}

void do_sh(uint8_t rt, uint8_t base, int16_t offset) {
    do_store(
        rt, base, offset, 2,
        [](uint32_t p, uint64_t v) {
            Utils::write_to_byte_array16(
                std::span<uint8_t>(rdram_data(), RDRAM_SIZE), p,
                static_cast<uint16_t>(v));
        },
        [](uint32_t p, uint64_t v) {
            Memory::write_paddr16(p, static_cast<uint16_t>(v));
        });
}

void do_sw(uint8_t rt, uint8_t base, int16_t offset) {
    do_store(
        rt, base, offset, 4,
        [](uint32_t p, uint64_t v) {
            Utils::write_to_byte_array32(
                std::span<uint8_t>(rdram_data(), RDRAM_SIZE), p,
                static_cast<uint32_t>(v));
        },
        [](uint32_t p, uint64_t v) {
            Memory::write_paddr32(p, static_cast<uint32_t>(v));
        });
}

void do_sd(uint8_t rt, uint8_t base, int16_t offset) {
    do_store(
        rt, base, offset, 8,
        [](uint32_t p, uint64_t v) {
            Utils::write_to_byte_array64(
                std::span<uint8_t>(rdram_data(), RDRAM_SIZE), p, v);
        },
        [](uint32_t p, uint64_t v) { Memory::write_paddr64(p, v); });
}

void do_swl(uint8_t rt, uint8_t base, int16_t offset) {
    auto &cpu = g_cpu();
    const uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr = Mmu::resolve_vaddr(
        static_cast<uint32_t>(vaddr), Mmu::BusAccess::STORE);
    if (paddr.has_value()) {
        const uint32_t shift = 8 * static_cast<uint32_t>((vaddr ^ 0) & 3);
        const uint32_t mask = 0xFFFFFFFFu >> shift;
        const uint32_t aligned = paddr.value() & ~3u;
        const uint32_t data =
            paddr_in_rdram(aligned, 4)
                ? Utils::read_from_byte_array32(
                      std::span<const uint8_t>(rdram_data(), RDRAM_SIZE),
                      aligned)
                : Memory::read_paddr32(aligned);
        const uint32_t reg = static_cast<uint32_t>(cpu.gpr.read(rt));
        const uint32_t out = (data & ~mask) | (reg >> shift);
        if (paddr_in_rdram(aligned, 4))
            Utils::write_to_byte_array32(
                std::span<uint8_t>(rdram_data(), RDRAM_SIZE), aligned, out);
        else
            Memory::write_paddr32(aligned, out);
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::STORE), 0, true);
        exec_state().aborted = true;
    }
}

void do_swr(uint8_t rt, uint8_t base, int16_t offset) {
    auto &cpu = g_cpu();
    const uint64_t vaddr = cpu.gpr.read(base) + offset;
    std::optional<uint32_t> paddr = Mmu::resolve_vaddr(
        static_cast<uint32_t>(vaddr), Mmu::BusAccess::STORE);
    if (paddr.has_value()) {
        const uint32_t shift = 8 * static_cast<uint32_t>((vaddr ^ 3) & 3);
        const uint32_t mask = 0xFFFFFFFFu << shift;
        const uint32_t aligned = paddr.value() & ~3u;
        const uint32_t data =
            paddr_in_rdram(aligned, 4)
                ? Utils::read_from_byte_array32(
                      std::span<const uint8_t>(rdram_data(), RDRAM_SIZE),
                      aligned)
                : Memory::read_paddr32(aligned);
        const uint32_t reg = static_cast<uint32_t>(cpu.gpr.read(rt));
        const uint32_t out = (data & ~mask) | (reg << shift);
        if (paddr_in_rdram(aligned, 4))
            Utils::write_to_byte_array32(
                std::span<uint8_t>(rdram_data(), RDRAM_SIZE), aligned, out);
        else
            Memory::write_paddr32(aligned, out);
    } else {
        cpu.handle_exception(
            g_tlb().get_tlb_exception_code(Mmu::BusAccess::STORE), 0, true);
        exec_state().aborted = true;
    }
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

void do_fpu(uint32_t raw) {
    instruction_t inst{};
    inst.raw = raw;
    auto &cpu = g_cpu();
    const bool exl_before = cpu.cop0.reg.status.exl != 0;
    cpu.execute_instruction(inst);
    if (!exl_before && cpu.cop0.reg.status.exl != 0)
        exec_state().aborted = true;
}

void do_bc1(uint32_t raw) {
    instruction_t inst{};
    inst.raw = raw;
    auto &cpu = g_cpu();
    // Keep JIT annul in sync with branch_likely_offset16 (Bc1l only).
    if (inst.r_type.rs == COP_BC) {
        const uint8_t ndtf = static_cast<uint8_t>(inst.i_type.rt);
        if (ndtf == COP1_BC_FL || ndtf == COP1_BC_TL) {
            const bool cmp = cpu.cop1.fcr31.compare != 0;
            const bool taken = (ndtf == COP1_BC_TL) ? cmp : !cmp;
            exec_state().annul_delay_slot = !taken;
        }
    }
    const bool exl_before = cpu.cop0.reg.status.exl != 0;
    cpu.execute_instruction(inst);
    if (!exl_before && cpu.cop0.reg.status.exl != 0)
        exec_state().aborted = true;
}

} // namespace Jit
} // namespace Cpu
} // namespace N64

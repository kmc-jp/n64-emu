#ifndef CPU_JIT_HELPERS_H
#define CPU_JIT_HELPERS_H

#include <cstdint>

namespace N64 {
namespace Cpu {
namespace Jit {

// Shared state for the currently executing compiled block.
struct ExecState {
    int cycles_done{0};
    bool aborted{false}; // exception / early exit
    // Set by branch-likely when the delay slot is annulled (not taken).
    bool annul_delay_slot{false};
};

ExecState &exec_state();
// Fixed address for the dynarec emitter (single-threaded CPU).
ExecState *exec_state_ptr();

// PC / delay-slot bookkeeping matching Cpu::step (without fetch).
void advance_pc();

// Interrupt / compare checks at block entry. Returns true if exception taken.
bool block_entry_checks();

void add_count(int n);

// Branch helpers (set delay_slot + next_pc like interpreter).
void do_branch_addr(bool cond, uint64_t target);
void do_branch_likely_addr(bool cond, uint64_t target);
void do_branch_offset(bool cond, int16_t offset);
void do_branch_likely_offset(bool cond, int16_t offset);
void do_link(uint8_t reg);
// J / JAL with delay-slot-out: region bits from branch PC (pc-4 after advance).
void do_j(uint32_t target26);
void do_jal(uint32_t target26);

uint64_t gpr_get(uint8_t n);
void gpr_set(uint8_t n, uint64_t v);
uint64_t get_hi();
uint64_t get_lo();
void set_hi(uint64_t v);
void set_lo(uint64_t v);
uint64_t get_pc();

void do_mult(uint8_t rs, uint8_t rt);
void do_multu(uint8_t rs, uint8_t rt);
void do_div(uint8_t rs, uint8_t rt);
void do_divu(uint8_t rs, uint8_t rt);

// Memory helpers. On TLB miss they take the exception and set aborted.
void do_lb(uint8_t rt, uint8_t base, int16_t offset);
void do_lbu(uint8_t rt, uint8_t base, int16_t offset);
void do_lh(uint8_t rt, uint8_t base, int16_t offset);
void do_lhu(uint8_t rt, uint8_t base, int16_t offset);
void do_lw(uint8_t rt, uint8_t base, int16_t offset);
void do_lwu(uint8_t rt, uint8_t base, int16_t offset);
void do_ld(uint8_t rt, uint8_t base, int16_t offset);
void do_lwl(uint8_t rt, uint8_t base, int16_t offset);
void do_lwr(uint8_t rt, uint8_t base, int16_t offset);
void do_sb(uint8_t rt, uint8_t base, int16_t offset);
void do_sh(uint8_t rt, uint8_t base, int16_t offset);
void do_sw(uint8_t rt, uint8_t base, int16_t offset);
void do_sd(uint8_t rt, uint8_t base, int16_t offset);
void do_swl(uint8_t rt, uint8_t base, int16_t offset);
void do_swr(uint8_t rt, uint8_t base, int16_t offset);

void do_mfc0(uint8_t rt, uint8_t rd);
void do_mtc0(uint8_t rt, uint8_t rd);
void do_dmfc0(uint8_t rt, uint8_t rd);
void do_dmtc0(uint8_t rt, uint8_t rd);

// COP1 / LWC1 / SWC1 / … via existing FpuImpl. Sets aborted on exception.
void do_fpu(uint32_t raw);
// BC1 with annul flag synced for Bc1l.
void do_bc1(uint32_t raw);

} // namespace Jit
} // namespace Cpu
} // namespace N64

#endif

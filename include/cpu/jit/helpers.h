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
};

ExecState &exec_state();

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

uint64_t gpr_get(uint8_t n);
void gpr_set(uint8_t n, uint64_t v);
uint64_t get_hi();
uint64_t get_lo();
void set_hi(uint64_t v);
void set_lo(uint64_t v);
uint64_t get_pc();

// Memory helpers. On TLB miss they take the exception and set aborted.
void do_lb(uint8_t rt, uint8_t base, int16_t offset);
void do_lbu(uint8_t rt, uint8_t base, int16_t offset);
void do_lh(uint8_t rt, uint8_t base, int16_t offset);
void do_lhu(uint8_t rt, uint8_t base, int16_t offset);
void do_lw(uint8_t rt, uint8_t base, int16_t offset);
void do_lwu(uint8_t rt, uint8_t base, int16_t offset);
void do_ld(uint8_t rt, uint8_t base, int16_t offset);
void do_sb(uint8_t rt, uint8_t base, int16_t offset);
void do_sh(uint8_t rt, uint8_t base, int16_t offset);
void do_sw(uint8_t rt, uint8_t base, int16_t offset);
void do_sd(uint8_t rt, uint8_t base, int16_t offset);

void do_mfc0(uint8_t rt, uint8_t rd);
void do_mtc0(uint8_t rt, uint8_t rd);
void do_dmfc0(uint8_t rt, uint8_t rd);
void do_dmtc0(uint8_t rt, uint8_t rd);

} // namespace Jit
} // namespace Cpu
} // namespace N64

#endif

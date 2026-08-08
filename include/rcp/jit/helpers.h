#ifndef RCP_JIT_HELPERS_H
#define RCP_JIT_HELPERS_H

#include <cstdint>

namespace N64 {
namespace Rsp {
namespace Jit {

// Full interpreter step for opcodes we do not emit (COP0/COP2 moves/...).
// Returns 1 if retired, 2 if IMEM was invalidated, 0 if already halted.
int exec_one(uint32_t inst);

// Thin VU callouts (instruction already decoded; no PC update / opcode switch).
void vu_compute(uint32_t inst);
void vu_lwc2(uint32_t inst);
void vu_swc2(uint32_t inst);
// Run a mixed streak of COP2 compute / LWC2 / SWC2 (no PC update).
void vu_run_vector_ops(const uint32_t *insts, uint32_t count);
// Decoded COP2 compute (tests / alternate emit).
void vu_compute_fields(uint32_t vd, uint32_t vs, uint32_t vt, uint32_t e,
                       uint32_t funct);

// BREAK side effects (halt/broke/interrupt).
void do_break();

// DMEM accessors (big-endian, addr wraps in 4KiB).
uint32_t mem_lw(uint32_t addr);
uint32_t mem_lh(uint32_t addr);
uint32_t mem_lb(uint32_t addr);
uint32_t mem_lhu(uint32_t addr);
uint32_t mem_lbu(uint32_t addr);
void mem_sw(uint32_t addr, uint32_t val);
void mem_sh(uint32_t addr, uint32_t val);
void mem_sb(uint32_t addr, uint32_t val);

} // namespace Jit
} // namespace Rsp
} // namespace N64

#endif

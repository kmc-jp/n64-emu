#pragma once

#include <cstdint>

namespace N64 {
namespace Cpu {

class Cpu;

// Call at the start of a half-line / Dynarec::run slice so idle warps stay
// within the remaining guest-cycle budget.
void idle_skip_begin_slice(int budget);

// Consume guest cycles from the idle budget (normal instruction progress).
void idle_skip_consume(int cycles);

// True if a warp is queued and waiting for soft-chain flush + apply.
bool idle_skip_pending();

// Apply any pending idle warp after flushing soft-chain pending time.
// Returns guest cycles warped (already added to COUNT + scheduler).
int idle_skip_apply_pending();

// Taken relative branch (imm in instruction). PC must be the delay-slot VA.
void idle_skip_check_relative(Cpu &cpu, int16_t imm);

// Taken absolute branch/jump to `target`. PC must be the delay-slot VA.
void idle_skip_check_absolute(Cpu &cpu, uint64_t target);

} // namespace Cpu
} // namespace N64

#include "cpu/idle_skip.h"
#include "cpu/cpu.h"
#include "memory/bus.h"
#include "mmu/mmu.h"
#include "n64_system/machine_advance.h"
#include "n64_system/scheduler.h"
#include <cstdint>
#include <limits>

namespace N64 {
namespace Cpu {

namespace {

struct IdleCtx {
    int budget_left = 0;
    int pending_skip = 0;
    bool active = false;
};

IdleCtx &ctx() {
    static IdleCtx c;
    return c;
}

uint32_t peek_delay_slot_word(Cpu &cpu) {
    const uint32_t va = static_cast<uint32_t>(cpu.get_pc64());
    if (auto direct = Mmu::try_direct_map(va))
        return Memory::read_paddr32(*direct);
    if (auto p = Mmu::resolve_vaddr(va))
        return Memory::read_paddr32(*p);
    return ~0u;
}

uint64_t cycles_until_compare(Cpu &cpu) {
    const uint64_t before = cpu.cop0.reg.count;
    const uint64_t target =
        (static_cast<uint64_t>(cpu.cop0.reg.compare) << 1) & 0x1FFFFFFFFULL;
    const uint64_t dist = (target - before) & 0x1FFFFFFFFULL;
    // dist==0 means we are sitting on the edge; let add_count handle the next
    // unit step rather than warping a full 33-bit wrap.
    return dist == 0 ? std::numeric_limits<uint64_t>::max() : dist;
}

void queue_idle_warp(Cpu &cpu) {
    auto &c = ctx();
    if (!c.active || c.pending_skip > 0)
        return;
    if (peek_delay_slot_word(cpu) != 0)
        return;

    // Mark only; distance is recomputed in apply after soft-chain flush so
    // COUNT/scheduler stay aligned.
    c.pending_skip = 1;
}

} // namespace

void idle_skip_begin_slice(int budget) {
    auto &c = ctx();
    c.budget_left = budget > 0 ? budget : 0;
    c.pending_skip = 0;
    c.active = true;
}

void idle_skip_consume(int cycles) {
    auto &c = ctx();
    if (!c.active || cycles <= 0)
        return;
    if (cycles >= c.budget_left)
        c.budget_left = 0;
    else
        c.budget_left -= cycles;
}

bool idle_skip_pending() {
    auto &c = ctx();
    return c.active && c.pending_skip > 0;
}

int idle_skip_apply_pending() {
    auto &c = ctx();
    if (!c.active || c.pending_skip <= 0)
        return 0;
    c.pending_skip = 0;

    uint64_t skip = g_scheduler().cycles_until_next_event();
    const uint64_t to_cmp = cycles_until_compare(g_cpu());
    if (to_cmp < skip)
        skip = to_cmp;
    if (c.budget_left > 0 && static_cast<uint64_t>(c.budget_left) < skip)
        skip = static_cast<uint64_t>(c.budget_left);

    if (skip < 4 || skip == std::numeric_limits<uint64_t>::max())
        return 0;

    constexpr uint64_t kMaxWarp = 1ull << 20;
    if (skip > kMaxWarp)
        skip = kMaxWarp;

    int warped = static_cast<int>(skip);
    g_cpu().add_count(static_cast<uint32_t>(warped));
    N64System::advance_after_cpu(warped);
    idle_skip_consume(warped);
    return warped;
}

void idle_skip_check_relative(Cpu &cpu, int16_t imm) {
    if (imm != -1)
        return;
    queue_idle_warp(cpu);
}

void idle_skip_check_absolute(Cpu &cpu, uint64_t target) {
    // At branch-exec time PC is the delay slot; branch VA is pc-4.
    if (target != cpu.get_pc64() - 4)
        return;
    queue_idle_warp(cpu);
}

} // namespace Cpu
} // namespace N64

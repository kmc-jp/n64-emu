#include "debugger/debugger.h"
#include "cpu/cop0.h"
#include "mmu/tlb.h"
#include "n64_system/scheduler.h"
#include "utils/log.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace N64 {
namespace Debugger {

Debugger Debugger::instance{};

Debugger &Debugger::get_instance() { return instance; }

void Debugger::configure(const N64System::Config &config) {
    enabled_ = config.debug;
    break_pcs = config.break_pcs;
    watch_paddrs.clear();
    break_on_tlb_ = false;
    break_on_any_exception_ = false;
    pause_requested_ = false;
    step_one_ = false;
    stop_before_next_ = false;
    skip_pc_break_once_ = false;
    pc_ring_count_ = pc_ring_next_ = 0;
    ex_ring_count_ = ex_ring_next_ = 0;

    if (!enabled_) {
        return;
    }

    Utils::info("Debugger enabled");
    for (uint32_t pc : break_pcs) {
        Utils::info("  break PC {:#010x}", pc);
    }
    if (!break_pcs.empty()) {
        // Stop when the first configured PC is reached.
    }
}

void Debugger::push_pc(uint32_t pc) {
    pc_ring[pc_ring_next_] = pc;
    pc_ring_next_ = (pc_ring_next_ + 1) % PC_RING_SIZE;
    if (pc_ring_count_ < PC_RING_SIZE) {
        pc_ring_count_++;
    }
}

void Debugger::push_exception(const ExceptionRecord &rec) {
    ex_ring[ex_ring_next_] = rec;
    ex_ring_next_ = (ex_ring_next_ + 1) % EX_RING_SIZE;
    if (ex_ring_count_ < EX_RING_SIZE) {
        ex_ring_count_++;
    }
}

bool Debugger::pc_breakpoint_hit(uint32_t pc) const {
    for (uint32_t bp : break_pcs) {
        if (bp == pc) {
            return true;
        }
    }
    return false;
}

bool Debugger::watch_hit(uint32_t paddr) const {
    for (uint32_t w : watch_paddrs) {
        // Match on 4-byte aligned address.
        if ((paddr & ~3u) == (w & ~3u)) {
            return true;
        }
    }
    return false;
}

void Debugger::request_pause(const char *reason) {
    if (!enabled_) {
        return;
    }
    pause_requested_ = true;
    Utils::info("Debugger pause: {}", reason);
}

void Debugger::on_exception(Cpu::ExceptionCode code, uint32_t vector) {
    if (!enabled_) {
        return;
    }

    auto &cop0 = g_cpu().cop0.reg;
    ExceptionRecord rec{};
    rec.code = static_cast<uint8_t>(code);
    rec.tlb_err = static_cast<int>(g_tlb().get_last_error());
    rec.bad_vaddr = cop0.bad_vaddr;
    rec.epc = cop0.epc;
    rec.entry_hi = cop0.entry_hi.raw;
    rec.context = cop0.context.raw;
    rec.xcontext = cop0.xcontext.raw;
    rec.vector = vector;
    rec.random = cop0.random;
    push_exception(rec);

    const bool is_tlb = code == Cpu::ExceptionCode::TLB_MISS_LOAD ||
                        code == Cpu::ExceptionCode::TLB_MISS_STORE ||
                        code == Cpu::ExceptionCode::TLB_MODIFICATION;
    if (break_on_any_exception_ || (break_on_tlb_ && is_tlb)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "exception code=%u vec=%#010x",
                      rec.code, vector);
        request_pause(buf);
    }
}

void Debugger::on_bus_access(uint32_t paddr, bool is_write) {
    if (!has_watches()) {
        return;
    }
    if (watch_hit(paddr)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "watch %s paddr=%#010x",
                      is_write ? "write" : "read", paddr);
        request_pause(buf);
    }
}

void Debugger::on_step() {
    if (!enabled_) {
        return;
    }

    // Called immediately before the CPU executes the instruction at PC.
    const uint32_t pc = static_cast<uint32_t>(g_cpu().get_pc64());
    push_pc(pc);

    // Finish a previous `step` command: stop before this instruction.
    if (stop_before_next_) {
        stop_before_next_ = false;
        enter_repl("step");
        // If user issued another `s` inside REPL, fall through below.
    }

    // Exception / watch requested a pause before this instruction.
    if (pause_requested_) {
        pause_requested_ = false;
        enter_repl("requested");
    }

    // PC breakpoint (skip if we just stopped for step/request on same PC).
    if (pc_breakpoint_hit(pc) && !skip_pc_break_once_) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "PC %#010x", pc);
        enter_repl(buf);
    }
    skip_pc_break_once_ = false;

    // `s` from REPL: run this instruction, stop before the next.
    if (step_one_) {
        step_one_ = false;
        stop_before_next_ = true;
        // Avoid immediately re-hitting the same PC break after one step away
        // and back — not needed here.
    }
}

void Debugger::enter_repl(const char *reason) {
    Utils::info("=== debugger stopped ({}) ===", reason);
    cmd_regs();
    std::cout << "(dbg) " << std::flush;

    std::string line;
    while (std::getline(std::cin, line)) {
        // Keep the window responsive while paused.
        SDL_PumpEvents();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                std::exit(0);
            }
        }

        if (handle_repl_line(line)) {
            return;
        }
        std::cout << "(dbg) " << std::flush;
    }

    // EOF on stdin: resume rather than hang forever.
    Utils::warn("Debugger: stdin EOF, continuing");
}

bool Debugger::handle_repl_line(const std::string &line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    if (cmd.empty()) {
        return false;
    }

    if (cmd == "c" || cmd == "continue") {
        // Continuing from a PC breakpoint should not instantly re-stop.
        skip_pc_break_once_ = true;
        return true;
    }
    if (cmd == "s" || cmd == "step") {
        step_one_ = true;
        skip_pc_break_once_ = true;
        return true;
    }
    if (cmd == "q" || cmd == "quit") {
        std::exit(0);
    }
    if (cmd == "regs") {
        cmd_regs();
        return false;
    }
    if (cmd == "cop0") {
        cmd_cop0();
        return false;
    }
    if (cmd == "tlb") {
        cmd_tlb();
        return false;
    }
    if (cmd == "bt") {
        cmd_bt();
        return false;
    }
    if (cmd == "ex") {
        cmd_ex();
        return false;
    }
    if (cmd == "break") {
        std::string arg;
        iss >> arg;
        if (arg == "tlb") {
            break_on_tlb_ = true;
            Utils::info("break on TLB exceptions enabled");
        } else if (arg == "exception" || arg == "exc") {
            break_on_any_exception_ = true;
            Utils::info("break on any exception enabled");
        } else if (!arg.empty()) {
            const uint32_t pc =
                static_cast<uint32_t>(std::strtoul(arg.c_str(), nullptr, 0));
            break_pcs.push_back(pc);
            Utils::info("added break PC {:#010x}", pc);
        } else {
            Utils::info("usage: break 0xPC | break tlb | break exception");
        }
        return false;
    }
    if (cmd == "watch") {
        std::string arg;
        iss >> arg;
        if (!arg.empty()) {
            const uint32_t paddr =
                static_cast<uint32_t>(std::strtoul(arg.c_str(), nullptr, 0));
            watch_paddrs.push_back(paddr);
            Utils::info("added watch paddr {:#010x}", paddr);
        } else {
            Utils::info("usage: watch 0xPADDR");
        }
        return false;
    }
    if (cmd == "help" || cmd == "h" || cmd == "?") {
        Utils::info(
            "commands: c/continue s/step regs cop0 tlb bt ex "
            "break watch q/quit");
        return false;
    }

    Utils::info("unknown command `{}` (try help)", cmd);
    return false;
}

void Debugger::cmd_regs() const {
    auto &cpu = g_cpu();
    Utils::info("PC={:#018x} HI={:#018x} LO={:#018x} time={:#x}",
                cpu.get_pc64(), cpu.hi, cpu.lo,
                g_scheduler().get_current_time());
    for (int i = 0; i < 32; i += 4) {
        Utils::info("{:>2}={:#018x}  {:>2}={:#018x}  {:>2}={:#018x}  "
                    "{:>2}={:#018x}",
                    Cpu::GPR_NAMES[i], cpu.gpr.read(i), Cpu::GPR_NAMES[i + 1],
                    cpu.gpr.read(i + 1), Cpu::GPR_NAMES[i + 2],
                    cpu.gpr.read(i + 2), Cpu::GPR_NAMES[i + 3],
                    cpu.gpr.read(i + 3));
    }
}

void Debugger::cmd_cop0() const {
    auto &r = g_cpu().cop0.reg;
    const uint64_t entry_hi = r.entry_hi.raw;
    const uint64_t ctx = r.context.raw;
    const uint64_t xctx = r.xcontext.raw;
    Utils::info("EPC={:#018x} BadVAddr={:#018x}", r.epc, r.bad_vaddr);
    Utils::info("Cause={:#010x} Status={:#010x} (exl={} ie={} im={:#04x})",
                r.cause.raw, r.status.raw, (unsigned)r.status.exl,
                (unsigned)r.status.ie, (unsigned)r.status.im);
    Utils::info("EntryHi={:#018x} Context={:#018x} XContext={:#018x}", entry_hi,
                ctx, xctx);
    Utils::info("Random={:#x} Wired={:#x} Index={:#010x} PageMask={:#010x}",
                r.random, r.wired, r.index, r.page_mask);
}

void Debugger::cmd_tlb() const {
    g_tlb().dump_entries();
}

void Debugger::cmd_bt() const {
    if (pc_ring_count_ == 0) {
        Utils::info("PC ring empty");
        return;
    }
    Utils::info("Recent PCs (oldest -> newest), {} entries:", pc_ring_count_);
    const size_t start =
        (pc_ring_next_ + PC_RING_SIZE - pc_ring_count_) % PC_RING_SIZE;
    for (size_t i = 0; i < pc_ring_count_; i++) {
        const size_t idx = (start + i) % PC_RING_SIZE;
        Utils::info("  [{:>3}] {:#010x}", i, pc_ring[idx]);
    }
}

void Debugger::cmd_ex() const {
    if (ex_ring_count_ == 0) {
        Utils::info("Exception ring empty");
        return;
    }
    Utils::info("Recent exceptions (oldest -> newest), {} entries:",
                ex_ring_count_);
    const size_t start =
        (ex_ring_next_ + EX_RING_SIZE - ex_ring_count_) % EX_RING_SIZE;
    for (size_t i = 0; i < ex_ring_count_; i++) {
        const size_t idx = (start + i) % EX_RING_SIZE;
        const auto &e = ex_ring[idx];
        Utils::info("  [{:>2}] code={} err={} BadV={:#018x} EPC={:#018x} "
                    "EntryHi={:#018x} vec={:#010x} Random={}",
                    i, e.code, e.tlb_err, e.bad_vaddr, e.epc, e.entry_hi,
                    e.vector, e.random);
    }
}

} // namespace Debugger

Debugger::Debugger &g_debugger() { return Debugger::Debugger::get_instance(); }

} // namespace N64

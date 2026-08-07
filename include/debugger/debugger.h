#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "cpu/cpu.h"
#include "n64_system/config.h"
#include <cstdint>
#include <string>
#include <vector>

namespace N64 {
namespace Debugger {

struct ExceptionRecord {
    uint8_t code{};
    int tlb_err{};
    uint64_t bad_vaddr{};
    uint64_t epc{};
    uint64_t entry_hi{};
    uint64_t context{};
    uint64_t xcontext{};
    uint32_t vector{};
    uint32_t random{};
};

class Debugger {
  public:
    void configure(const N64System::Config &config);

    bool enabled() const { return enabled_; }

    bool has_watches() const { return enabled_ && !watch_paddrs.empty(); }

    // Called once per CPU instruction from the system step callback.
    void on_step();

    // Record exception; may request pause (honored on next on_step).
    void on_exception(Cpu::ExceptionCode code, uint32_t vector);

    // Physical bus watch (32-bit). is_write distinguishes R/W.
    void on_bus_access(uint32_t paddr, bool is_write);

    static Debugger &get_instance();

  private:
    bool enabled_{false};
    bool pause_requested_{false};
    bool step_one_{false};
    bool stop_before_next_{false};
    bool skip_pc_break_once_{false};
    bool break_on_tlb_{false};
    bool break_on_any_exception_{false};

    std::vector<uint32_t> break_pcs;
    std::vector<uint32_t> watch_paddrs;

    static constexpr size_t PC_RING_SIZE = 256;
    static constexpr size_t EX_RING_SIZE = 64;
    uint32_t pc_ring[PC_RING_SIZE]{};
    size_t pc_ring_count_{0};
    size_t pc_ring_next_{0};

    ExceptionRecord ex_ring[EX_RING_SIZE]{};
    size_t ex_ring_count_{0};
    size_t ex_ring_next_{0};

    void request_pause(const char *reason);
    void enter_repl(const char *reason);
    bool handle_repl_line(const std::string &line);

    void cmd_regs() const;
    void cmd_cop0() const;
    void cmd_tlb() const;
    void cmd_bt() const;
    void cmd_ex() const;

    void push_pc(uint32_t pc);
    void push_exception(const ExceptionRecord &rec);

    bool pc_breakpoint_hit(uint32_t pc) const;
    bool watch_hit(uint32_t paddr) const;

    static Debugger instance;
};

} // namespace Debugger

Debugger::Debugger &g_debugger();

} // namespace N64

#endif

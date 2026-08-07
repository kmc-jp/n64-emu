#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "cpu/cpu.h"
#include "n64_system/config.h"
#include <cstdint>
#include <optional>
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

struct WatchEntry {
    uint32_t paddr{};
    bool write_only{false};
};

class Debugger {
  public:
    void configure(const N64System::Config &config);

    bool enabled() const { return enabled_; }

    bool has_watches() const { return enabled_ && !watches_.empty(); }

    // Called once per CPU instruction from the system step callback.
    void on_step();

    // Record exception; may request pause (honored on next on_step).
    void on_exception(Cpu::ExceptionCode code, uint32_t vector);

    // Physical bus watch (32-bit). is_write distinguishes R/W.
    void on_bus_access(uint32_t paddr, bool is_write);

    // COP0 MTC0/DMTC0 (and any Reg::write) watch.
    void on_cop0_write(uint8_t reg_num, uint64_t value);

    // Called when RSP halt is cleared (SP_STATUS clear_halt).
    void on_rsp_unhalt();

    static Debugger &get_instance();

  private:
    bool enabled_{false};
    bool pause_requested_{false};
    bool step_one_{false};
    bool stop_before_next_{false};
    bool skip_pc_break_once_{false};
    bool break_on_tlb_{false};
    bool break_on_any_exception_{false};
    bool break_on_cop0_any_{false};
    // -1 = none; otherwise match this COP0 register number only.
    int break_on_cop0_reg_{-1};
    // <0 = disabled; otherwise pause when scheduler time reaches this.
    int64_t break_at_time_{-1};
    // <0 = any type; otherwise match OSTask type in DMEM 0xFC0 on unhalt.
    int break_sp_task_type_{-2}; // -2 = disabled

    std::vector<uint32_t> break_pcs;
    std::vector<WatchEntry> watches_;

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
    void cmd_mem(uint32_t vaddr, int words) const;
    void cmd_pmem(uint32_t paddr, int words) const;
    void cmd_vi() const;
    void cmd_mi() const;
    void cmd_rsp() const;
    void cmd_dpc() const;
    void cmd_ost(const std::vector<uint32_t> &tbaddrs) const;
    void cmd_mq(uint32_t vaddr) const;
    void cmd_scan_task(uint32_t type, uint32_t limit) const;
    void cmd_find(uint32_t word, uint32_t limit) const;

    void push_pc(uint32_t pc);
    void push_exception(const ExceptionRecord &rec);

    bool pc_breakpoint_hit(uint32_t pc) const;
    std::optional<WatchEntry> watch_hit(uint32_t paddr, bool is_write) const;

    static uint32_t read_vaddr32(uint32_t vaddr);
    static uint32_t read_rdram32(uint32_t paddr);
    static const char *osthread_state_name(uint16_t state);

    static Debugger instance;
};

} // namespace Debugger

Debugger::Debugger &g_debugger();

} // namespace N64

#endif

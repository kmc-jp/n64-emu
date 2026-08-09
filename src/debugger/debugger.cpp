#include "debugger/debugger.h"
#include "cpu/cop0.h"
#include "memory/bus.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "mmio/mi.h"
#include "mmio/vi.h"
#include "mmu/mmu.h"
#include "mmu/tlb.h"
#include "n64_system/scheduler.h"
#include "rcp/dpc.h"
#include "rcp/rsp.h"
#include "utils/byte_array.h"
#include "utils/log.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace N64 {
namespace Debugger {

namespace {

template <typename... Args>
void dbg_out(fmt::format_string<Args...> fmt, Args &&...args) {
    const std::string s = fmt::format(fmt, std::forward<Args>(args)...);
    std::cout << s << std::endl;
}

} // namespace

Debugger Debugger::instance{};

Debugger &Debugger::get_instance() { return instance; }

void Debugger::configure(const N64System::Config &config) {
    enabled_ = config.debug;
    break_pcs = config.break_pcs;
    watches_.clear();
    for (uint32_t p : config.watch_paddrs) {
        watches_.push_back(WatchEntry{p, false});
    }
    break_on_tlb_ = false;
    break_on_any_exception_ = false;
    break_on_cop0_any_ = false;
    break_on_cop0_reg_ = -1;
    break_at_time_ = config.break_after_cycles > 0
                         ? static_cast<int64_t>(config.break_after_cycles)
                         : -1;
    break_sp_task_type_ = -2;
    pause_requested_ = false;
    step_one_ = false;
    stop_before_next_ = false;
    skip_pc_break_once_ = false;
    pc_ring_count_ = pc_ring_next_ = 0;
    ex_ring_count_ = ex_ring_next_ = 0;

    if (!enabled_) {
        return;
    }

    dbg_out("Debugger enabled");
    for (uint32_t pc : break_pcs) {
        dbg_out("  break PC {:#010x}", pc);
    }
    if (break_at_time_ >= 0) {
        dbg_out("  break at time {:#x}", break_at_time_);
    }
    for (const auto &w : watches_) {
        dbg_out("  watch paddr {:#010x}", w.paddr);
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

std::optional<WatchEntry> Debugger::watch_hit(uint32_t paddr,
                                              bool is_write) const {
    for (const auto &w : watches_) {
        if ((paddr & ~3u) != (w.paddr & ~3u)) {
            continue;
        }
        if (w.write_only && !is_write) {
            continue;
        }
        return w;
    }
    return std::nullopt;
}

uint32_t Debugger::read_vaddr32(uint32_t vaddr) {
    auto paddr = Mmu::resolve_vaddr(vaddr);
    if (!paddr.has_value()) {
        return 0;
    }
    return Memory::read_paddr32(paddr.value());
}

uint32_t Debugger::read_rdram32(uint32_t paddr) {
    paddr &= RDRAM_SIZE_MASK;
    if (paddr + 3 >= RDRAM_SIZE) {
        return 0;
    }
    return Memory::read_paddr32(paddr);
}

const char *Debugger::osthread_state_name(uint16_t state) {
    switch (state) {
    case 1:
        return "STOPPED";
    case 2:
        return "RUNNABLE";
    case 4:
        return "RUNNING";
    case 8:
        return "WAITING";
    default:
        return "?";
    }
}

void Debugger::request_pause(const char *reason) {
    if (!enabled_) {
        return;
    }
    pause_requested_ = true;
    dbg_out("Debugger pause: {}", reason);
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
    if (auto hit = watch_hit(paddr, is_write)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "watch %s paddr=%#010x",
                      is_write ? "write" : "read", paddr);
        request_pause(buf);
    }
}

void Debugger::on_cop0_write(uint8_t reg_num, uint64_t value) {
    if (!enabled_) {
        return;
    }
    if (!break_on_cop0_any_ &&
        !(break_on_cop0_reg_ >= 0 &&
          break_on_cop0_reg_ == static_cast<int>(reg_num))) {
        return;
    }

    auto &cpu = g_cpu();
    auto &r = cpu.cop0.reg;
    const char *name = (reg_num < Cpu::COP0_REG_NAMES.size())
                           ? Cpu::COP0_REG_NAMES[reg_num].data()
                           : "?";
    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "COP0 write %s(%u)=%#018llx @ prev_pc=%#018llx "
                  "Status=%#010x Cause=%#010x",
                  name, reg_num, static_cast<unsigned long long>(value),
                  static_cast<unsigned long long>(cpu.get_prev_pc64()),
                  r.status.raw, r.cause.raw);
    request_pause(buf);
}

void Debugger::on_rsp_unhalt() {
    if (!enabled_ || break_sp_task_type_ == -2) {
        return;
    }
    const uint32_t type = g_rsp().dmem_load32(0xFC0);
    if (break_sp_task_type_ >= 0 &&
        type != static_cast<uint32_t>(break_sp_task_type_)) {
        return;
    }
    char buf[96];
    std::snprintf(buf, sizeof(buf), "RSP unhalt task type=%u", type);
    request_pause(buf);
}

void Debugger::on_step() {
    if (!enabled_) {
        return;
    }

    // Called immediately before the CPU executes the instruction at PC.
    const uint32_t pc = static_cast<uint32_t>(g_cpu().get_pc64());
    push_pc(pc);

    if (break_at_time_ >= 0 &&
        static_cast<int64_t>(g_scheduler().get_current_time()) >=
            break_at_time_) {
        break_at_time_ = -1;
        enter_repl("time");
    }

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
    }
}

void Debugger::enter_repl(const char *reason) {
    dbg_out("=== debugger stopped ({}) ===", reason);
    cmd_regs();
    std::cout << "(dbg) " << std::flush;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (handle_repl_line(line)) {
            return;
        }
        std::cout << "(dbg) " << std::flush;
    }

    // EOF on stdin: resume rather than hang forever.
    dbg_out("Debugger: stdin EOF, continuing");
}

bool Debugger::handle_repl_line(const std::string &line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    if (cmd.empty()) {
        return false;
    }

    if (cmd == "c" || cmd == "continue") {
        std::string after;
        iss >> after;
        if (!after.empty()) {
            // continue for N more cycles, then stop
            char *end = nullptr;
            const unsigned long n = std::strtoul(after.c_str(), &end, 0);
            if (end != after.c_str() && *end == '\0') {
                break_at_time_ =
                    static_cast<int64_t>(g_scheduler().get_current_time() + n);
                dbg_out("continue until time {:#x}", break_at_time_);
            } else {
                dbg_out("usage: c [cycles]");
                return false;
            }
        }
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
    if (cmd == "vi") {
        cmd_vi();
        return false;
    }
    if (cmd == "mi") {
        cmd_mi();
        return false;
    }
    if (cmd == "rsp") {
        cmd_rsp();
        return false;
    }
    if (cmd == "dpc") {
        cmd_dpc();
        return false;
    }
    if (cmd == "ost" || cmd == "threads") {
        std::vector<uint32_t> addrs;
        std::string a;
        while (iss >> a) {
            addrs.push_back(
                static_cast<uint32_t>(std::strtoul(a.c_str(), nullptr, 0)));
        }
        if (addrs.empty()) {
            dbg_out("usage: ost <tcb_addr>...");
            return false;
        }
        cmd_ost(addrs);
        return false;
    }
    if (cmd == "mq") {
        std::string addr_s;
        iss >> addr_s;
        if (addr_s.empty()) {
            dbg_out("usage: mq 0xVADDR");
            return false;
        }
        cmd_mq(static_cast<uint32_t>(std::strtoul(addr_s.c_str(), nullptr, 0)));
        return false;
    }
    if (cmd == "mem") {
        std::string addr_s;
        int words = 16;
        iss >> addr_s;
        if (addr_s.empty()) {
            dbg_out("usage: mem 0xVADDR [words]");
            return false;
        }
        iss >> words;
        if (words <= 0) {
            words = 16;
        }
        if (words > 64) {
            words = 64;
        }
        const uint32_t vaddr =
            static_cast<uint32_t>(std::strtoul(addr_s.c_str(), nullptr, 0));
        cmd_mem(vaddr, words);
        return false;
    }
    if (cmd == "pmem") {
        std::string addr_s;
        int words = 16;
        iss >> addr_s;
        if (addr_s.empty()) {
            dbg_out("usage: pmem 0xPADDR [words]");
            return false;
        }
        iss >> words;
        if (words <= 0) {
            words = 16;
        }
        if (words > 64) {
            words = 64;
        }
        const uint32_t paddr =
            static_cast<uint32_t>(std::strtoul(addr_s.c_str(), nullptr, 0));
        cmd_pmem(paddr, words);
        return false;
    }
    if (cmd == "scan") {
        std::string what;
        iss >> what;
        if (what == "task") {
            uint32_t type = 1;
            uint32_t limit = 32;
            iss >> type >> limit;
            cmd_scan_task(type, limit);
        } else {
            dbg_out("usage: scan task [type=1] [limit=32]");
        }
        return false;
    }
    if (cmd == "find") {
        std::string word_s;
        iss >> word_s;
        if (word_s.empty()) {
            dbg_out("usage: find 0xWORD [limit]");
            return false;
        }
        uint32_t limit = 64;
        iss >> limit;
        cmd_find(
            static_cast<uint32_t>(std::strtoul(word_s.c_str(), nullptr, 0)),
            limit);
        return false;
    }
    if (cmd == "break") {
        std::string arg;
        iss >> arg;
        if (arg == "tlb") {
            break_on_tlb_ = true;
            dbg_out("break on TLB exceptions enabled");
        } else if (arg == "exception" || arg == "exc") {
            break_on_any_exception_ = true;
            dbg_out("break on any exception enabled");
        } else if (arg == "after") {
            std::string n_s;
            iss >> n_s;
            if (n_s.empty()) {
                dbg_out("usage: break after <cycles>");
            } else {
                const unsigned long n = std::strtoul(n_s.c_str(), nullptr, 0);
                break_at_time_ =
                    static_cast<int64_t>(g_scheduler().get_current_time() + n);
                dbg_out("break after {} cycles (at time {:#x})", n,
                        break_at_time_);
            }
        } else if (arg == "time") {
            std::string n_s;
            iss >> n_s;
            if (n_s.empty()) {
                dbg_out("usage: break time <abs_cycles>");
            } else {
                break_at_time_ = static_cast<int64_t>(
                    std::strtoull(n_s.c_str(), nullptr, 0));
                dbg_out("break at time {:#x}", break_at_time_);
            }
        } else if (arg == "sp-task" || arg == "sptask") {
            std::string t_s;
            iss >> t_s;
            if (t_s.empty() || t_s == "any") {
                break_sp_task_type_ = -1;
                dbg_out("break on any RSP unhalt");
            } else {
                break_sp_task_type_ =
                    static_cast<int>(std::strtol(t_s.c_str(), nullptr, 0));
                dbg_out("break on RSP unhalt type={}", break_sp_task_type_);
            }
        } else if (arg == "cop0") {
            std::string which;
            iss >> which;
            if (which.empty() || which == "any") {
                break_on_cop0_any_ = true;
                break_on_cop0_reg_ = -1;
                dbg_out("break on any COP0 write enabled");
            } else if (which == "epc") {
                break_on_cop0_any_ = false;
                break_on_cop0_reg_ = Cpu::Cop0Reg::EPC;
                dbg_out("break on COP0 EPC write enabled");
            } else {
                const int reg =
                    static_cast<int>(std::strtol(which.c_str(), nullptr, 0));
                if (reg >= 0 && reg < 32) {
                    break_on_cop0_any_ = false;
                    break_on_cop0_reg_ = reg;
                    dbg_out("break on COP0 reg {} write enabled", reg);
                } else {
                    dbg_out("usage: break cop0 [epc|any|<regnum>]");
                }
            }
        } else if (!arg.empty()) {
            const uint32_t pc =
                static_cast<uint32_t>(std::strtoul(arg.c_str(), nullptr, 0));
            break_pcs.push_back(pc);
            dbg_out("added break PC {:#010x}", pc);
        } else {
            dbg_out(
                "usage: break 0xPC | break tlb | break exception | "
                "break after <n> | break time <n> | break sp-task [type|any] | "
                "break cop0 [epc|any|<regnum>]");
        }
        return false;
    }
    if (cmd == "watch") {
        std::string arg;
        iss >> arg;
        bool write_only = false;
        if (arg == "w" || arg == "write") {
            write_only = true;
            iss >> arg;
        }
        if (!arg.empty()) {
            const uint32_t paddr =
                static_cast<uint32_t>(std::strtoul(arg.c_str(), nullptr, 0));
            watches_.push_back(WatchEntry{paddr, write_only});
            dbg_out("added {}watch paddr {:#010x}", write_only ? "write-" : "",
                    paddr);
        } else {
            dbg_out("usage: watch [w] 0xPADDR");
        }
        return false;
    }
    if (cmd == "help" || cmd == "h" || cmd == "?") {
        dbg_out(
            "commands: c/continue [cycles] s/step regs cop0 tlb bt ex "
            "mem pmem vi mi rsp dpc ost/threads mq scan find "
            "break[ tlb|exception|after|time|sp-task|cop0] watch[ w] q/quit");
        return false;
    }

    dbg_out("unknown command `{}` (try help)", cmd);
    return false;
}

void Debugger::cmd_regs() const {
    auto &cpu = g_cpu();
    dbg_out("PC={:#018x} HI={:#018x} LO={:#018x} time={:#x}", cpu.get_pc64(),
            cpu.hi, cpu.lo, g_scheduler().get_current_time());
    for (int i = 0; i < 32; i += 4) {
        dbg_out("{:>2}={:#018x}  {:>2}={:#018x}  {:>2}={:#018x}  "
                "{:>2}={:#018x}",
                Cpu::GPR_NAMES[i], cpu.gpr.read(i), Cpu::GPR_NAMES[i + 1],
                cpu.gpr.read(i + 1), Cpu::GPR_NAMES[i + 2], cpu.gpr.read(i + 2),
                Cpu::GPR_NAMES[i + 3], cpu.gpr.read(i + 3));
    }
}

void Debugger::cmd_cop0() const {
    auto &r = g_cpu().cop0.reg;
    const uint64_t entry_hi = r.entry_hi.raw;
    const uint64_t ctx = r.context.raw;
    const uint64_t xctx = r.xcontext.raw;
    dbg_out("EPC={:#018x} BadVAddr={:#018x}", r.epc, r.bad_vaddr);
    dbg_out("Cause={:#010x} Status={:#010x} (exl={} ie={} im={:#04x})",
            r.cause.raw, r.status.raw, (unsigned)r.status.exl,
            (unsigned)r.status.ie, (unsigned)r.status.im);
    dbg_out("EntryHi={:#018x} Context={:#018x} XContext={:#018x}", entry_hi,
            ctx, xctx);
    dbg_out("Random={:#x} Wired={:#x} Index={:#010x} PageMask={:#010x}",
            r.random, r.wired, r.index, r.page_mask);
    dbg_out("Compare={:#010x} Count={:#010x} (internal={:#018x})", r.compare,
            static_cast<uint32_t>(r.count >> 1), r.count);
}

void Debugger::cmd_tlb() const { g_tlb().dump_entries(); }

void Debugger::cmd_bt() const {
    if (pc_ring_count_ == 0) {
        dbg_out("PC ring empty");
        return;
    }
    dbg_out("Recent PCs (oldest -> newest), {} entries:", pc_ring_count_);
    const size_t start =
        (pc_ring_next_ + PC_RING_SIZE - pc_ring_count_) % PC_RING_SIZE;
    for (size_t i = 0; i < pc_ring_count_; i++) {
        const size_t idx = (start + i) % PC_RING_SIZE;
        dbg_out("  [{:>3}] {:#010x}", i, pc_ring[idx]);
    }
}

void Debugger::cmd_ex() const {
    if (ex_ring_count_ == 0) {
        dbg_out("Exception ring empty");
        return;
    }
    dbg_out("Recent exceptions (oldest -> newest), {} entries:",
            ex_ring_count_);
    const size_t start =
        (ex_ring_next_ + EX_RING_SIZE - ex_ring_count_) % EX_RING_SIZE;
    for (size_t i = 0; i < ex_ring_count_; i++) {
        const size_t idx = (start + i) % EX_RING_SIZE;
        const auto &e = ex_ring[idx];
        dbg_out("  [{:>2}] code={} err={} BadV={:#018x} EPC={:#018x} "
                "EntryHi={:#018x} Context={:#018x} XContext={:#018x} "
                "vec={:#010x} Random={}",
                i, e.code, e.tlb_err, e.bad_vaddr, e.epc, e.entry_hi, e.context,
                e.xcontext, e.vector, e.random);
    }
}

void Debugger::cmd_mem(uint32_t vaddr, int words) const {
    for (int i = 0; i < words; i++) {
        const uint32_t va = vaddr + static_cast<uint32_t>(i * 4);
        auto paddr = Mmu::resolve_vaddr(va);
        if (!paddr.has_value()) {
            dbg_out("{:#010x}: <unmapped>", va);
            continue;
        }
        const uint32_t w = Memory::read_paddr32(paddr.value());
        dbg_out("{:#010x}: {:#010x}", va, w);
    }
}

void Debugger::cmd_pmem(uint32_t paddr, int words) const {
    for (int i = 0; i < words; i++) {
        const uint32_t pa = paddr + static_cast<uint32_t>(i * 4);
        const uint32_t w = Memory::read_paddr32(pa);
        dbg_out("{:#010x}: {:#010x}", pa, w);
    }
}

void Debugger::cmd_vi() const {
    auto &vi = g_vi();
    dbg_out("VI CTRL={:#010x} ORIGIN={:#010x} WIDTH={:#x} INTR={:#x} "
            "CURRENT={:#x}",
            vi.reg_status, vi.reg_origin, vi.reg_width, vi.reg_intr,
            vi.reg_current);
    dbg_out("VI V_SYNC={:#x} H_SYNC={:#x} H_VIDEO={:#010x} V_VIDEO={:#010x}",
            vi.reg_vsync, vi.reg_hsync, vi.reg_h_video, vi.reg_v_video);
    dbg_out("VI X_SCALE={:#010x} Y_SCALE={:#010x} half_lines={} "
            "cyc/half={}",
            vi.reg_x_scale, vi.reg_y_scale, vi.num_half_lines,
            vi.cycles_per_half_line);
}

void Debugger::cmd_mi() const {
    auto &mi = g_mi();
    const auto intr = mi.get_reg_intr();
    const auto mask = mi.get_reg_intr_mask();
    dbg_out("MI INTR={:#010x} (sp={} si={} ai={} vi={} pi={} dp={})", intr.raw,
            (unsigned)intr.sp, (unsigned)intr.si, (unsigned)intr.ai,
            (unsigned)intr.vi, (unsigned)intr.pi, (unsigned)intr.dp);
    dbg_out("MI MASK={:#010x} (sp={} si={} ai={} vi={} pi={} dp={})", mask.raw,
            (unsigned)mask.sp, (unsigned)mask.si, (unsigned)mask.ai,
            (unsigned)mask.vi, (unsigned)mask.pi, (unsigned)mask.dp);
}

void Debugger::cmd_rsp() const {
    auto &rsp = g_rsp();
    const uint32_t status = rsp.read_paddr32(Rsp::PADDR_SP_STATUS);
    const uint32_t pc = rsp.read_paddr32(Rsp::PADDR_SP_PC);
    dbg_out("SP STATUS={:#010x} PC={:#05x} halt={} broke={} iob={}", status, pc,
            (status & 1u) != 0, (status & 2u) != 0, (status & 0x40u) != 0);
    // OSTask as u32 fields (0x40 bytes @ DMEM 0xFC0).
    const uint32_t type = rsp.dmem_load32(0xFC0);
    const uint32_t flags = rsp.dmem_load32(0xFC4);
    const uint32_t boot = rsp.dmem_load32(0xFC8);
    const uint32_t boot_sz = rsp.dmem_load32(0xFCC);
    const uint32_t ucode = rsp.dmem_load32(0xFD0);
    const uint32_t ucode_sz = rsp.dmem_load32(0xFD4);
    const uint32_t udata = rsp.dmem_load32(0xFD8);
    const uint32_t udata_sz = rsp.dmem_load32(0xFDC);
    const uint32_t stack = rsp.dmem_load32(0xFE0);
    const uint32_t stack_sz = rsp.dmem_load32(0xFE4);
    const uint32_t output = rsp.dmem_load32(0xFE8);
    const uint32_t output_sz = rsp.dmem_load32(0xFEC);
    const uint32_t data = rsp.dmem_load32(0xFF0);
    const uint32_t data_sz = rsp.dmem_load32(0xFF4);
    const uint32_t yield_p = rsp.dmem_load32(0xFF8);
    const uint32_t yield_sz = rsp.dmem_load32(0xFFC);
    dbg_out("OSTask type={} flags={:#x}", type, flags);
    dbg_out("  boot={:#010x} sz={:#x} ucode={:#010x} sz={:#x}", boot, boot_sz,
            ucode, ucode_sz);
    dbg_out("  udata={:#010x} sz={:#x} stack={:#010x} sz={:#x}", udata,
            udata_sz, stack, stack_sz);
    dbg_out("  output={:#010x} sz={:#x} data={:#010x} sz={:#x}", output,
            output_sz, data, data_sz);
    dbg_out("  yield={:#010x} sz={:#x}", yield_p, yield_sz);
}

void Debugger::cmd_dpc() const {
    auto &dpc = g_dpc();
    dbg_out("DPC START={:#010x} END={:#010x} CURRENT={:#010x} STATUS={:#010x}",
            dpc.get_start(), dpc.get_end(), dpc.get_current(),
            dpc.get_status().raw);
}

void Debugger::cmd_ost(const std::vector<uint32_t> &tbaddrs) const {
    dbg_out("OSThreads (libultra layout):");
    for (size_t i = 0; i < tbaddrs.size(); i++) {
        const uint32_t base = tbaddrs[i] & RDRAM_SIZE_MASK;
        // Accept either physical or KSEG0 virtual.
        const uint32_t p = (base >= 0x80000000u) ? (base & 0x1FFFFFFFu) : base;
        const uint32_t next = read_rdram32(p + 0x00);
        const uint32_t pri = read_rdram32(p + 0x04);
        const uint32_t queue = read_rdram32(p + 0x08);
        const uint32_t state_flags = read_rdram32(p + 0x10);
        const uint16_t state = static_cast<uint16_t>(state_flags >> 16);
        const uint16_t flags = static_cast<uint16_t>(state_flags & 0xFFFF);
        const uint32_t id = read_rdram32(p + 0x14);
        const uint32_t thr_pc = read_rdram32(p + 0x110);
        dbg_out("  TCB {:#010x}: id={} pri={} state={}({}) flags={:#x} "
                "queue={:#010x} next={:#010x} ctx.pc={:#010x}",
                p, id, pri, state, osthread_state_name(state), flags, queue,
                next, thr_pc);
    }
}

void Debugger::cmd_mq(uint32_t vaddr) const {
    // OSMesgQueue: mtqueue, fullqueue, validCount, first, msgCount, msg*
    const uint32_t mtqueue = read_vaddr32(vaddr + 0x00);
    const uint32_t fullqueue = read_vaddr32(vaddr + 0x04);
    const int32_t valid = static_cast<int32_t>(read_vaddr32(vaddr + 0x08));
    const int32_t first = static_cast<int32_t>(read_vaddr32(vaddr + 0x0C));
    const int32_t msgCount = static_cast<int32_t>(read_vaddr32(vaddr + 0x10));
    const uint32_t msg = read_vaddr32(vaddr + 0x14);
    dbg_out("OSMesgQueue {:#010x}: valid={}/{} first={} mtqueue={:#010x} "
            "fullqueue={:#010x} msg*={:#010x}",
            vaddr, valid, msgCount, first, mtqueue, fullqueue, msg);
    const int show = valid > 8 ? 8 : (valid < 0 ? 0 : valid);
    for (int i = 0; i < show; i++) {
        const int idx = (first + i) % (msgCount > 0 ? msgCount : 1);
        const uint32_t mesg =
            read_vaddr32(msg + static_cast<uint32_t>(idx * 4));
        dbg_out("  msg[{}]={:#010x}", idx, mesg);
    }
}

void Debugger::cmd_scan_task(uint32_t type, uint32_t limit) const {
    auto &rdram = g_memory().get_rdram();
    uint32_t found = 0;
    dbg_out("Scanning RDRAM for OSTask type={} ...", type);
    // OSTask is 0x40 bytes; type at +0. Scan 8-byte aligned candidates.
    for (uint32_t p = 0; p + 0x40 <= RDRAM_SIZE; p += 8) {
        const uint32_t t = Utils::read_from_byte_array32(rdram, p);
        if (t != type) {
            continue;
        }
        const uint32_t flags = Utils::read_from_byte_array32(rdram, p + 4);
        // Heuristic: flags usually small; ucode_boot pointer in RDRAM/KSEG.
        const uint32_t boot_hi = Utils::read_from_byte_array32(rdram, p + 8);
        if (boot_hi != 0 && boot_hi != 0xFFFFFFFF) {
            continue;
        }
        dbg_out("  candidate {:#010x}: type={} flags={:#010x}", p, t, flags);
        if (++found >= limit) {
            dbg_out("  (limit {})", limit);
            break;
        }
    }
    dbg_out("scan done, {} hit(s)", found);
}

void Debugger::cmd_find(uint32_t word, uint32_t limit) const {
    auto &rdram = g_memory().get_rdram();
    uint32_t found = 0;
    dbg_out("Finding {:#010x} in RDRAM ...", word);
    for (uint32_t p = 0; p + 4 <= RDRAM_SIZE; p += 4) {
        const uint32_t w = Utils::read_from_byte_array32(rdram, p);
        if (w != word) {
            continue;
        }
        dbg_out("  {:#010x}", p);
        if (++found >= limit) {
            dbg_out("  (limit {})", limit);
            break;
        }
    }
    dbg_out("find done, {} hit(s)", found);
}

} // namespace Debugger

Debugger::Debugger &g_debugger() { return Debugger::Debugger::get_instance(); }

} // namespace N64

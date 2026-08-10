#include "n64_system/n64_system.h"
#include "audio/audio.h"
#include "cpu/cached_interp.h"
#if defined(N64_JIT_X64)
#include "cpu/jit/jit.h"
#endif
#include "debugger/debugger.h"
#include "memory/bus.h"
#include "memory/memory.h"
#include "mmio/ai.h"
#include "mmio/mi.h"
#include "mmio/pi.h"
#include "mmio/si.h"
#include "mmio/vi.h"
#include "mmu/tlb.h"
#include "n64_system/config.h"
#include "n64_system/interrupt.h"
#include "n64_system/scheduler.h"
#include "rcp/dpc.h"
#include "rcp/rsp.h"
#include "rcp/vu_profile.h"
#include "utils/log.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace N64 {
namespace N64System {

namespace {
void pace_field_realtime() {
    using clock = std::chrono::steady_clock;
    static clock::time_point deadline{};
    static bool have_deadline = false;
    constexpr auto kField =
        std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<double>(1.0 / 60.0));

    const auto now = clock::now();
    if (!have_deadline) {
        deadline = now + kField;
        have_deadline = true;
        return;
    }
    if (now < deadline) {
        const auto wait_t0 = clock::now();
        const auto spin_from = deadline - std::chrono::milliseconds(1);
        if (clock::now() < spin_from)
            std::this_thread::sleep_until(spin_from);
        while (clock::now() < deadline) {
        }
        Audio::note_sync_wait_ns(
            static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    clock::now() - wait_t0)
                    .count()));
    }
    auto next = deadline + kField;
    const auto t = clock::now();
    while (next <= t)
        next += kField;
    deadline = next;
}
} // namespace

namespace {
FieldPresentFn g_field_present = nullptr;
PresentStatsFn g_present_stats = nullptr;
} // namespace

void set_field_present(FieldPresentFn fn) { g_field_present = fn; }
void set_present_stats_fn(PresentStatsFn fn) { g_present_stats = fn; }

static void reset_all(Config &config) {
    N64::g_scheduler().init();

    N64::g_memory().reset();
    N64::g_memory().load_rom(config.rom_filepath);
    N64::g_tlb().reset();
    N64::g_cpu().reset();
#if defined(N64_JIT_X64)
    if (config.cpu_backend == CpuBackend::Jit)
        N64::Cpu::Jit::g_dynarec().reset();
    else
        N64::Cpu::CachedInterp::reset();
#else
    N64::Cpu::CachedInterp::reset();
#endif
    N64::g_rsp().reset();
    N64::g_dpc().reset();
    N64::g_pi().reset();
    N64::g_si().reset();
    N64::g_mi().reset();
    N64::g_ai().reset();
    N64::g_vi().reset();
}

void set_up(Config &config) {
    Utils::info("Starting N64 system");
    N64System::reset_all(config);
    g_debugger().configure(config);

    if (config.test_mode) {
        Utils::info("Copying ROM");
        for (uint32_t i = 0; i < 0x100000; i += 4) {
            uint32_t data = Memory::read_paddr32(0x10001000 + i);
            Memory::write_paddr32(0x00001000 + i, data);
        }
        Utils::info("Set pc to 0x80001000");
        N64::g_cpu().set_pc64(0x80001000);
        Utils::info("Skipped Bootcode");
    } else {
        Utils::debug("Executing PIF ROM");
        N64::g_si().pif.execute_rom_hle();
    }
}

void shutdown() {
    Utils::info("Stopping N64 system");
    N64::g_memory().persist_sram();
}

static void cpu_step_callback(Config &config) {
    if (config.test_mode) {
        if (N64::g_cpu().gpr.read(30) != 0) {
            Utils::info("Test finished");
            Utils::core_dump();
            if ((int64_t)N64::g_cpu().gpr.read(30) == -1) {
                Utils::info("Test passed");
                exit(0);
            } else {
                Utils::info("Test failed");
                exit(-1);
            }
        }
    }

    if constexpr (Utils::LOG_INSTRUCTION) {
        if (N64::g_scheduler().get_current_time() % 0x10'0000 == 0) {
            Utils::set_log_level(Utils::LogLevel::TRACE);
            Utils::debug("");
            Utils::debug("Current CPU time: 0x{:016X}. showing next trace log",
                         N64::g_scheduler().get_current_time());
            Utils::debug("pc = {:#18x}", N64::g_cpu().get_pc64());
        } else if (N64::g_scheduler().get_current_time() % 0x10'0000 == 1) {
            Utils::set_log_level(config.log_level);
        }
    }
}

void step(Config &config) {
    static const bool profile_frame = [] {
        const char *e = getenv("N64_PROFILE_FRAME");
        return e && e[0] != '\0' && e[0] != '0';
    }();
    static uint64_t prof_fields = 0;
    static double prof_emu_ms = 0.0;
    static double prof_cpu_ms = 0.0;
    static double prof_rsp_ms = 0.0;
    static double prof_rdp_ms = 0.0;
    static double prof_audio_ms = 0.0;
    static auto prof_last_log = std::chrono::steady_clock::now();

    for (int field = 0; field < g_vi().get_num_fields(); field++) {
        const auto field_t0 = profile_frame ? std::chrono::steady_clock::now()
                                            : std::chrono::steady_clock::time_point{};
        for (int line = 0; line < g_vi().get_num_half_lines(); line++) {
            g_vi().set_reg_current(line * 2 + field);
            if ((g_vi().get_reg_current() & 0x3FE) == g_vi().get_reg_intr()) {
                g_mi().get_reg_intr().vi = 1;
                N64System::check_interrupt();
            }

            int remaining = g_vi().get_cycles_per_half_line();
            const bool use_jit =
#if defined(N64_JIT_X64)
                config.cpu_backend == CpuBackend::Jit;
#else
                false;
#endif
            Debugger::Debugger &dbg = g_debugger();
            const bool dbg_on = dbg.enabled();
            const bool need_step_cb =
                config.test_mode || Utils::LOG_INSTRUCTION;

            double cpu_ms = 0.0;
            while (remaining > 0) {
                int taken = 1;
                if (dbg_on)
                    dbg.on_step();
                const auto cpu_t0 = profile_frame
                                       ? std::chrono::steady_clock::now()
                                       : std::chrono::steady_clock::time_point{};
                if (use_jit) {
#if defined(N64_JIT_X64)
                    taken = Cpu::Jit::g_dynarec().run(remaining);
                    if (taken < 1)
                        taken = 1;
#endif
                } else if (dbg_on || need_step_cb) {
                    g_cpu().step();
                    taken = static_cast<int>(Cpu::CPU_CYCLES_PER_INST);
                    g_scheduler().tick(static_cast<uint64_t>(taken));
                } else {
                    taken = Cpu::CachedInterp::run(remaining);
                    if (taken < 1)
                        taken = 1;
                }
                if (profile_frame) {
                    cpu_ms += std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - cpu_t0)
                                  .count();
                }

                if (need_step_cb)
                    cpu_step_callback(config);

                remaining -= taken;
            }
            if (profile_frame)
                prof_cpu_ms += cpu_ms;
        }
        if ((g_vi().get_reg_current() & 0x3FE) == g_vi().get_reg_intr()) {
            g_mi().get_reg_intr().vi = 1;
            N64System::check_interrupt();
        }
        const auto rdp_t0 = profile_frame ? std::chrono::steady_clock::now()
                                          : std::chrono::steady_clock::time_point{};
        if (g_field_present)
            g_field_present(g_vi());
        const auto rdp_t1 = profile_frame ? std::chrono::steady_clock::now()
                                          : std::chrono::steady_clock::time_point{};
        pace_field_realtime();
        if (profile_frame) {
            const auto t1 = std::chrono::steady_clock::now();
            prof_emu_ms +=
                std::chrono::duration<double, std::milli>(rdp_t0 - field_t0)
                    .count();
            prof_rdp_ms +=
                std::chrono::duration<double, std::milli>(rdp_t1 - rdp_t0)
                    .count();
            prof_audio_ms += Audio::take_sync_wait_ms();
            ++prof_fields;
            if (std::chrono::duration<double>(t1 - prof_last_log).count() >=
                1.0) {
                const double inv = prof_fields ? 1.0 / prof_fields : 0.0;
                const double emu = prof_emu_ms * inv;
                const double pace = prof_audio_ms * inv;
                const double cpu = std::max(prof_cpu_ms * inv, 0.0);
                const double rsp = prof_rsp_ms * inv;
                const double rdp = prof_rdp_ms * inv;
                const PresentCounters present =
                    g_present_stats ? g_present_stats() : PresentCounters{};
                Utils::info(
                    "frame profile: fields/s={} presents/s={} skipped/s={} "
                    "avg cpu={:.2f}ms rsp={:.2f}ms rdp={:.2f}ms "
                    "pace={:.2f}ms total={:.2f}ms "
                    "(cpu {:.0f}% / rsp {:.0f}% / rdp {:.0f}% / pace {:.0f}%) "
                    "work={:.2f}ms budget60={:.2f}ms",
                    prof_fields, present.presented, present.skipped, cpu, rsp,
                    rdp, pace, emu + rdp + pace,
                    (emu + rdp + pace) > 0 ? 100.0 * cpu / (emu + rdp + pace)
                                          : 0.0,
                    (emu + rdp + pace) > 0 ? 100.0 * rsp / (emu + rdp + pace)
                                          : 0.0,
                    (emu + rdp + pace) > 0 ? 100.0 * rdp / (emu + rdp + pace)
                                          : 0.0,
                    (emu + rdp + pace) > 0 ? 100.0 * pace / (emu + rdp + pace)
                                          : 0.0,
                    cpu + rsp + rdp, 1000.0 / 60.0);
                Rsp::vu_profile_dump();
#if defined(N64_JIT_X64)
                if (config.cpu_backend == CpuBackend::Jit)
                    Cpu::Jit::jit_profile_dump();
#endif
                prof_fields = 0;
                prof_emu_ms = 0.0;
                prof_cpu_ms = 0.0;
                prof_rsp_ms = 0.0;
                prof_rdp_ms = 0.0;
                prof_audio_ms = 0.0;
                prof_last_log = t1;
            }
        }
    }
}

} // namespace N64System
} // namespace N64

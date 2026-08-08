#include "n64_system/n64_system.h"
#include "app/parallel_rdp_wrapper.h"
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
#include "rcp/rsp_thread.h"
#include "rcp/vu_profile.h"
#include "utils/log.h"
#include <chrono>
#include <cstdlib>

namespace N64 {
namespace N64System {

static void reset_all(Config &config) {
    // this is not an actual hardware. but reset here.
    N64::g_scheduler().init();

    // reset all hardware
    N64::g_memory().reset();
    N64::g_memory().load_rom(config.rom_filepath);
    N64::g_tlb().reset();
    N64::g_cpu().reset();
#if defined(N64_JIT_X64)
    N64::Cpu::Jit::g_dynarec().reset();
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

    Rsp::g_rsp_thread().configure(config.rsp_thread);
    Rsp::g_rsp_thread().start();
    if (config.rsp_thread)
        Utils::info("RSP worker thread enabled");

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
        // PIF ROM execution
        Utils::debug("Executing PIF ROM");
        N64::g_si().pif.execute_rom_hle();
    }
}

static void cpu_step_callback(Config &config) {
    // Check condition for n64-tests
    // https://github.com/Dillonb/n64-tests
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

    // For debugging
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

// https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/system/n64system.c#L313
void step(Config &config, Vulkan::WSI *wsi) {
    static int consumed_cpu_cycles = 0;
    // N64_PROFILE_FRAME=1: wall-clock split of emu vs RDP/present per VI field.
    static const bool profile_frame = [] {
        const char *e = getenv("N64_PROFILE_FRAME");
        return e && e[0] != '\0' && e[0] != '0';
    }();
    static uint64_t prof_fields = 0;
    static double prof_emu_ms = 0.0;
    static double prof_cpu_ms = 0.0;
    static double prof_rsp_ms = 0.0;
    static double prof_rdp_ms = 0.0;
    static auto prof_last_log = std::chrono::steady_clock::now();

    for (int field = 0; field < g_vi().get_num_fields(); field++) {
        const auto field_t0 = profile_frame ? std::chrono::steady_clock::now()
                                            : std::chrono::steady_clock::time_point{};
        for (int line = 0; line < g_vi().get_num_half_lines(); line++) {
            // TODO: why this value?
            g_vi().set_reg_current(line * 2 + field);
            if ((g_vi().get_reg_current() & 0x3FE) == g_vi().get_reg_intr()) {
                g_mi().get_reg_intr().vi = 1;
                N64System::check_interrupt();
            }

            // FIXME: what if a CPU step take more than one cycle?
            int remaining = g_vi().get_cycles_per_half_line();
            const bool use_jit =
#if defined(N64_JIT_X64)
                config.cpu_backend == CpuBackend::Jit;
#else
                false;
#endif
            auto &rsp = g_rsp();
            auto &sched = g_scheduler();
            Debugger::Debugger &dbg = g_debugger();
            const bool dbg_on = dbg.enabled();
            const bool need_step_cb =
                config.test_mode || Utils::LOG_INSTRUCTION;

            double cpu_ms = 0.0;
            double rsp_ms = 0.0;
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
                } else {
                    g_cpu().step();
                    taken = static_cast<int>(Cpu::CPU_CYCLES_PER_INST);
                }
                if (profile_frame) {
                    cpu_ms += std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - cpu_t0)
                                  .count();
                }

                consumed_cpu_cycles += taken;
                if (need_step_cb)
                    cpu_step_callback(config);

                // RSP step. RSP ticks 2/3x faster than CPU.
                // With --rsp-thread the worker owns execution; only re-kick
                // if still unhalted after a quantum.
                const auto rsp_t0 = profile_frame
                                       ? std::chrono::steady_clock::now()
                                       : std::chrono::steady_clock::time_point{};
                if (config.rsp_thread) {
                    if (!rsp.halted())
                        Rsp::g_rsp_thread().kick_until_halt();
                } else {
                    while (consumed_cpu_cycles >= 3) {
                        consumed_cpu_cycles -= 3;
                        rsp.step();
                        rsp.step();
                    }
                }
                if (profile_frame) {
                    rsp_ms += std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - rsp_t0)
                                  .count();
                }

                sched.tick(static_cast<uint64_t>(taken));
                remaining -= taken;
            }
            if (profile_frame) {
                prof_cpu_ms += cpu_ms;
                prof_rsp_ms += rsp_ms;
            }
        }
        if ((g_vi().get_reg_current() & 0x3FE) == g_vi().get_reg_intr()) {
            g_mi().get_reg_intr().vi = 1;
            N64System::check_interrupt();
        }
        if (config.rsp_thread) {
            const auto rsp_wait_t0 =
                profile_frame ? std::chrono::steady_clock::now()
                              : std::chrono::steady_clock::time_point{};
            Rsp::g_rsp_thread().wait_idle();
            if (!g_rsp().halted())
                Rsp::g_rsp_thread().kick_until_halt();
            if (profile_frame) {
                prof_rsp_ms += std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - rsp_wait_t0)
                                   .count();
            }
        }
        const auto rdp_t0 = profile_frame ? std::chrono::steady_clock::now()
                                          : std::chrono::steady_clock::time_point{};
        if (wsi)
            PRDPWrapper::update_screen(*wsi, g_vi());
        if (profile_frame) {
            const auto t1 = std::chrono::steady_clock::now();
            prof_emu_ms +=
                std::chrono::duration<double, std::milli>(rdp_t0 - field_t0)
                    .count();
            prof_rdp_ms +=
                std::chrono::duration<double, std::milli>(t1 - rdp_t0).count();
            ++prof_fields;
            if (std::chrono::duration<double>(t1 - prof_last_log).count() >=
                1.0) {
                const double inv = prof_fields ? 1.0 / prof_fields : 0.0;
                const double emu = prof_emu_ms * inv;
                const double cpu = prof_cpu_ms * inv;
                const double rsp = prof_rsp_ms * inv;
                const double rdp = prof_rdp_ms * inv;
                Utils::info(
                    "frame profile: fields/s≈{} avg cpu={:.2f}ms rsp={:.2f}ms "
                    "rdp={:.2f}ms total={:.2f}ms (cpu {:.0f}% / rsp {:.0f}% / "
                    "rdp {:.0f}%) budget60={:.2f}ms",
                    prof_fields, cpu, rsp, rdp, emu + rdp,
                    (emu + rdp) > 0 ? 100.0 * cpu / (emu + rdp) : 0.0,
                    (emu + rdp) > 0 ? 100.0 * rsp / (emu + rdp) : 0.0,
                    (emu + rdp) > 0 ? 100.0 * rdp / (emu + rdp) : 0.0,
                    1000.0 / 60.0);
                Rsp::vu_profile_dump();
                prof_fields = 0;
                prof_emu_ms = 0.0;
                prof_cpu_ms = 0.0;
                prof_rsp_ms = 0.0;
                prof_rdp_ms = 0.0;
                prof_last_log = t1;
            }
        }
    }
}

} // namespace N64System
} // namespace N64

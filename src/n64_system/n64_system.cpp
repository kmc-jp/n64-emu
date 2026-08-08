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
#include "utils/log.h"

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

    // Boot progress sampling (every ~1M cycles); skip when debugger owns tracing
    if (!config.debug) {
        static bool left_ipl3 = false;
        static bool entered_game = false;
        const uint64_t t = N64::g_scheduler().get_current_time();
        if (t % 0x10'0000 == 0) {
            const uint32_t pc = static_cast<uint32_t>(N64::g_cpu().get_pc64());
            Utils::debug("pc sample = {:#010x}", pc);
            if (!left_ipl3 && (pc < 0xA4000000 || pc > 0xA4001FFF)) {
                left_ipl3 = true;
                Utils::info("Left IPL3 (DMEM). pc = {:#010x}", pc);
            }
            if (!entered_game && (pc & 0xFFF00000) == 0x80100000) {
                entered_game = true;
                Utils::info("Entered game code region. pc = {:#010x}", pc);
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

    for (int field = 0; field < g_vi().get_num_fields(); field++) {
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
            while (remaining > 0) {
                int taken = 1;
                // Debugger hooks before each run unit (insn or JIT block).
                g_debugger().on_step();
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

                consumed_cpu_cycles += taken;
                cpu_step_callback(config);

                // RSP step. RSP ticks 2/3x faster than CPU
                while (consumed_cpu_cycles >= 3) {
                    consumed_cpu_cycles -= 3;
                    g_rsp().step();
                    g_rsp().step();
                }

                g_scheduler().tick(static_cast<uint64_t>(taken));
                remaining -= taken;
            }
        }
        if ((g_vi().get_reg_current() & 0x3FE) == g_vi().get_reg_intr()) {
            g_mi().get_reg_intr().vi = 1;
            N64System::check_interrupt();
        }
        if (wsi)
            PRDPWrapper::update_screen(*wsi, g_vi());

        // TODO: AI step
    }
}

} // namespace N64System
} // namespace N64

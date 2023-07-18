#include "config.h"
#include "cpu/cpu.h"
#include "memory/memory.h"
#include "memory/pif.h"
#include "mmio/pi.h"
#include "rsp/rsp.h"
#include "scheduler.h"

namespace N64 {
namespace N64System {

void run(Config config) {
    Utils::info("Resetting N64 system");
    N64::g_scheduler().init();

    // Reset all processors
    N64::g_memory().load_rom(config.rom_filepath);
    N64::g_memory().reset();
    N64::g_cpu().reset();
    N64::g_rsp().reset();
    N64::g_pi().reset();
    N64::g_si().reset();

#if BOOTCODE_SKIP == 1
    Utils::info("Copying ROM");
    for (uint32_t i = 0; i < 0x100000; i += 4) {
        uint32_t data = N64::Memory::read_paddr32(0x10001000 + i);
        N64::Memory::write_paddr32(0x00001000 + i, data);
    }
    Utils::info("Set pc to 0x80001000");
    N64::g_cpu().set_pc64(0x80001000);
    Utils::info("Skipped Bootcode");
#else
    // PIF ROM execution
    Utils::info("Executing PIF ROM");
    N64::Memory::pif_rom_execute();
#endif

    Utils::info("Starting N64 system");
    int consumed_cpu_cycles = 0;
    while (true) {
        N64::g_cpu().step();
        consumed_cpu_cycles += N64::Cpu::CPU_CYCLES_PER_INST;

        // RSP ticks 2/3x faster than CPU
        while (consumed_cpu_cycles >= 3) {
            consumed_cpu_cycles -= 3;
            N64::g_rsp().step();
            N64::g_rsp().step();
        }

        g_scheduler().tick(N64::Cpu::CPU_CYCLES_PER_INST);

        /*
        if (g_cpu().get_pc64() == (0xffffffff800001c8 - 0x98)) {
            Utils::set_log_level(Utils::LogLevel::TRACE);
            Utils::critical("here! pc = {:#18x}", g_cpu().get_pc64());
        }
        */

#if DILLON_TEST == 1
        if (g_cpu().gpr.read(30) != 0) {
            Utils::core_dump();
            exit(-1);
        }
#endif

        if (g_scheduler().get_current_time() % 0x10'0000 == 0) {
            Utils::set_log_level(Utils::LogLevel::TRACE);
            Utils::debug("");
            Utils::debug("Current CPU time: 0x{:016X}. showing next trace log",
                         g_scheduler().get_current_time());
            Utils::debug("pc = {:#18x}", N64::g_cpu().get_pc64());
        } else if (g_scheduler().get_current_time() % 0x10'0000 == 1) {
            Utils::set_log_level(config.log_level);
        }
    }
}

} // namespace N64System
} // namespace N64
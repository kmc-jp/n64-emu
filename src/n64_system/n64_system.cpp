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

    // PIF ROM execution
    Utils::info("Executing PIF ROM");
    N64::Memory::pif_rom_execute();

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
    }
}

} // namespace N64System
} // namespace N64
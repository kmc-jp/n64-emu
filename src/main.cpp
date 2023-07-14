#include "cpu/cpu.h"
#include "memory/memory.h"
#include "memory/pif.h"
#include "mmio/pi.h"
#include "rsp/rsp.h"
#include <spdlog/spdlog.h>

const std::string USAGE = "USAGE: n64 <ROM.z64>";

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << USAGE << std::endl;
        return -1;
    }
    // set logger level
    spdlog::set_level(spdlog::level::trace);
    // use custom logging pattern
    // https://github.com/gabime/spdlog/issues/2073
    spdlog::set_pattern("[%^%l%$] %v");

    std::string filepath = {argv[1]};

    N64::g_memory().load_rom(filepath);
    N64::g_memory().reset();
    N64::g_cpu().reset();
    N64::g_rsp().reset();
    N64::g_pi().reset();

    N64::Memory::pif_rom_execute();

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
    }

    return 0;
}

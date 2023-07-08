﻿#include "cpu/cpu.h"
#include "memory/memory.h"
#include "memory/pif.h"
#include "rsp/rsp.h"
#include <iostream>
#include <spdlog/spdlog.h>

const std::string USAGE = "USAGE: n64 <ROM.z64>";

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << USAGE << std::endl;
        return -1;
    }
    // ロガーのレベルをdebugに設定
    spdlog::set_level(spdlog::level::debug);
    // タイムスタンプが邪魔なのでカスタムのパターンを用いる
    spdlog::set_pattern("[%l] %v");

    std::string filepath = {argv[1]};

    N64::n64cpu.init();
    N64::n64mem.init_with_rom(filepath);
    N64::n64rsp.init();

    N64::Memory::pif_rom_execute();

    int consumed_cpu_cycles = 0;
    while (true) {
        N64::n64cpu.step();
        consumed_cpu_cycles += N64::Cpu::CPU_CYCLES_PER_INST;

        // RSP ticks 2/3x faster than CPU
        while (consumed_cpu_cycles >= 3) {
            consumed_cpu_cycles -= 3;
            N64::n64rsp.step();
            N64::n64rsp.step();
        }
    }

    return 0;
}

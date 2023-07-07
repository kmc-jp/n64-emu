#include "cpu/cpu.h"
#include "memory/memory.h"
#include "memory/pif.h"
#include "utils/logger.h"

#include <cstdint>
#include <iostream>

const std::string USAGE = "USAGE: n64 <ROM.z64>";

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << USAGE << std::endl;
        return -1;
    }

    std::string filepath = {argv[1]};
    N64::n64mem.init_with_rom(filepath);

    N64::pif_rom_execute();

    N64::n64cpu.dump();

    N64::gLogger.Debug("ddaaaa");
    
    return 0;
}

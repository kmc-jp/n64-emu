#ifndef CONFIG_H
#define CONFIG_H

#include "utils/log.h"
#include <cstdint>
#include <string>
#include <vector>

namespace N64 {
namespace N64System {

struct Config {
    std::string rom_filepath{};
    std::string log_filepath{};
    Utils::LogLevel log_level;
    bool test_mode;
    bool debug{false};
    std::vector<uint32_t> break_pcs{};
    std::vector<uint32_t> watch_paddrs{};
    // >0: enter debugger when scheduler time reaches this (from boot).
    uint64_t break_after_cycles{0};
};

bool read_config_from_command_line(Config &config, int argc, char *argv[]);

} // namespace N64System
} // namespace N64

#endif

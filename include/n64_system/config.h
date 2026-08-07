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
};

bool read_config_from_command_line(Config &config, int argc, char *argv[]);

} // namespace N64System
} // namespace N64

#endif

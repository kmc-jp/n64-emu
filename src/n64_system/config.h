#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include "utils.h"

namespace N64 {
namespace N64System {

struct Config {
    std::string rom_filepath{};
    std::string log_filepath{};
    Utils::LogLevel log_level;
};

bool read_config_from_command_line(Config &config, int argc, char *argv[]);

} // namespace N64System
} // namespace N64

#endif
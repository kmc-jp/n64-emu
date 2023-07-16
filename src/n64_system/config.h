#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace N64 {
namespace N64System {

struct Config {
    std::string rom_filepath{};
    std::string log_filepath{};
};

bool read_config_from_command_line(Config &config, int argc, char *argv[]);

} // namespace N64System
} // namespace N64

#endif
#include "config.h"
#include "utils/utils.h"
#include <iostream>
#include <string>

namespace N64 {
namespace N64System {

bool read_config_from_command_line(Config &config, int argc, char *argv[]) {
    if (argc < 2)
        return false;

    // default to debug
    config.log_level = Utils::LogLevel::DEBUG;
    // default to false
    config.test_mode = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view current = argv[i];
        if (current == "--log") {
            // ログファイル指定
            if (config.log_filepath.empty() == false) {
                std::cerr << "Error: log file already specified" << std::endl;
                return false;
            }
            config.log_filepath = argv[i + 1];
            i++;
        } else if (current.starts_with("--log-level=")) {
            // ログレベル指定
            std::string_view level_str =
                current.substr(std::string("--log-level=").size());
            if (level_str == "debug")
                config.log_level = Utils::LogLevel::DEBUG;
            else if (level_str == "info")
                config.log_level = Utils::LogLevel::INFO;
            else if (level_str == "warn")
                config.log_level = Utils::LogLevel::WARN;
            else if (level_str == "trace")
                config.log_level = Utils::LogLevel::TRACE;
            else if (level_str == "critical")
                config.log_level = Utils::LogLevel::CRITICAL;
            else if (level_str == "off")
                config.log_level = Utils::LogLevel::OFF;
            else {
                std::cerr << "Error: unknown log level `" << level_str << "`"
                          << std::endl;
                return false;
            }
        } else if (current == "--test") {
            config.test_mode = true;
        } else if (current.empty() == false && !current.starts_with('-')) {
            // ROMパス指定
            if (config.rom_filepath.empty() == false) {
                std::cerr << "Error: ROM file already specified" << std::endl;
                return false;
            }
            config.rom_filepath = current;
        } else {
            std::cerr << "Error: unknown argument `" << current << "`"
                      << std::endl;
            return false;
        }
    }

    return true;
}

} // namespace N64System
} // namespace N64
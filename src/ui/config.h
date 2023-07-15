#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace N64 {
namespace Ui {

struct Config {
    std::string rom_filepath{};
    std::string log_filepath{};
};

constexpr std::string_view COMMAND_LINE_USAGE =
    "Usage: n64 [options] <ROM.z64>\n"
    "Options:\n"
    "--log <file>\tspecify output log file\n";

bool apply_config_from_command_line(Config &config, int argc, char *argv[]) {
    if (argc < 2)
        return false;

    for (int i = 1; i < argc; ++i) {
        if (const std::string_view read = argv[i]; read == "--log") {
            // ログファイル指定
            if (config.log_filepath.empty() == false)
                return false;
            config.log_filepath = argv[i + 1];
            i++;
        } else if (read.empty() == false && read[0] != '-') {
            // ROMパス指定
            if (config.rom_filepath.empty() == false)
                return false;
            config.rom_filepath = read;
        } else {
            return false;
        }
    }

    return true;
}

} // namespace Ui
} // namespace N64

#endif
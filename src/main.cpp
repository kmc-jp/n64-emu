#include "app/app.h"
#include "n64_system/config.h"
#include "utils/utils.h"
#include <iostream>

constexpr std::string_view USAGE =
    "Usage: n64 [options] <ROM.z64>\n"
    "Options:\n"
    "--log <file>\tspecify output log file(default to stdout)\n"
    "--log-level [trace|debug|info|critical|off]\tset log level(default to "
    "debug)\n";

int main(int argc, char *argv[]) {
    N64::N64System::Config config{};

    if (read_config_from_command_line(config, argc, argv) == false) {
        std::cout << USAGE << std::endl;
        return -1;
    }

    Utils::init_logger();
    if (config.log_filepath.empty() == false) {
        Utils::set_log_file(config.log_filepath);
    }
    Utils::set_log_level(config.log_level);

    N64::Frontend::App app(config);
    app.run();

    return 0;
}
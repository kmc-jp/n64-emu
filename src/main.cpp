#include "config.h"
#include "n64_system/n64_system.h"
#include "n64_system/config.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <iostream>

constexpr std::string_view USAGE = "Usage: n64 [options] <ROM.z64>\n"
                                   "Options:\n"
                                   "--log <file>\tspecify output log file\n";

int main(int argc, char *argv[]) {
    N64::N64System::Config config{};

    if (read_config_from_command_line(config, argc, argv) == false) {
        std::cout << USAGE << std::endl;
        return -1;
    }

    if (config.log_filepath.empty() == false) {
        set_default_logger(
            spdlog::basic_logger_mt("basic_logger", config.log_filepath, true));
    }

    // set logger level
    spdlog::set_level(spdlog::level::trace);
    // use custom logging pattern
    // https://github.com/gabime/spdlog/issues/2073
    spdlog::set_pattern("[%^%l%$] %v");

    N64::N64System::run(config);

    return 0;
}
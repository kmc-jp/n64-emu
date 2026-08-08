#include "app/app.h"
#include "n64_system/config.h"
#include "utils/log.h"
#include <iostream>

constexpr std::string_view USAGE =
    "Usage: n64 [options] <ROM.z64>\n"
    "Options:\n"
    "--log <file>\tspecify output log file(default to stdout)\n"
    "--log-level=[trace|debug|info|critical|off]\tset log level (default to "
    "info)\n"
    "--interpreter\tuse CPU interpreter (default)\n"
    "--jit\tuse CPU dynarec (x86-64)\n"
    "--headless\tno window / no Vulkan present\n"
    "--test\trun n64-tests (implies --headless)\n";

// Entry point of the n64-emu. Handles command line arguments and starts the
// App.
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

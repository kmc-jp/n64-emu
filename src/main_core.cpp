#include "n64_system/config.h"
#include "ui/app_core.h"
#include "ui/config_cli.h"
#include "utils/log.h"
#include <iostream>

constexpr std::string_view USAGE =
    "Usage: kamo64-core [options] <ROM.z64>\n"
    "CLI / windowed frontend without ImGui (for tests and scripting).\n"
    "Options:\n"
    "--log <file>\tspecify output log file(default to stdout)\n"
    "--log-level=[trace|debug|info|critical|off]\tset log level (default to "
    "info)\n"
    "--jit\tuse CPU dynarec (x86-64, default)\n"
    "--no-jit\tdisable CPU dynarec (use interpreter)\n"
    "--upscale=[1|2|4|8]\tParallel-RDP resolution multiplier (default 4)\n"
    "--frame-interp\tenable frame interpolation (default: optical flow)\n"
    "--no-frame-interp\tdisable frame interpolation (default)\n"
    "--headless\tno window / no Vulkan present\n"
    "--test\trun n64-tests (implies --headless)\n"
    "--debug\tenable interactive debugger\n"
    "--break=ADDR\tbreak when PC hits ADDR (implies --debug)\n"
    "--break-after=N\tbreak after N scheduler cycles (implies --debug)\n"
    "--watch=PADDR\twatch physical bus access (implies --debug)\n";

int main(int argc, char *argv[]) {
    N64::N64System::Config config{};

    Utils::init_logger();

    if (!N64::Ui::apply_command_line(config, argc, argv) ||
        config.rom_filepath.empty()) {
        std::cout << USAGE << std::endl;
        return -1;
    }

    if (!config.log_filepath.empty())
        Utils::set_log_file(config.log_filepath);
    Utils::set_log_level(config.log_level);

    N64::Ui::AppCore app(config);
    app.run();
    return 0;
}

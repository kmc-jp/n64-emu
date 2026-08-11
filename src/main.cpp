#include "memory/memory.h"
#include "n64_system/config.h"
#include "ui/app.h"
#include "ui/config_cli.h"
#include "ui/config_toml.h"
#include "utils/log.h"
#include <iostream>

constexpr std::string_view USAGE =
    "Usage: kamo64 [options] [ROM.z64]\n"
    "ImGui GUI frontend (menu bar + file dialog).\n"
    "Options:\n"
    "--log <file>\tspecify output log file(default to stdout)\n"
    "--log-level=[trace|debug|info|critical|off]\tset log level (default to "
    "info)\n"
    "--jit\tuse CPU dynarec (x86-64, default)\n"
    "--no-jit\tdisable CPU dynarec (use interpreter)\n"
    "--upscale=[1|2|4|8]\tParallel-RDP resolution multiplier (default 4)\n"
    "--frame-interp\tenable frame interpolation (default: optical flow)\n"
    "--no-frame-interp\tdisable frame interpolation (default)\n"
    "--debug\tenable interactive debugger\n"
    "--break=ADDR\tbreak when PC hits ADDR (implies --debug)\n"
    "--break-after=N\tbreak after N scheduler cycles (implies --debug)\n"
    "--watch=PADDR\twatch physical bus access (implies --debug)\n"
    "\nWithout a ROM path, opens the GUI menu.\n"
    "For headless/CI tests use kamo64-core instead.\n";

int main(int argc, char *argv[]) {
    N64::N64System::Config config{};
    N64::Ui::UiSettings ui_settings{};

    Utils::init_logger();

    N64::Ui::load_toml(config, ui_settings);
    if (!N64::Ui::apply_command_line(config, argc, argv)) {
        std::cout << USAGE << std::endl;
        return -1;
    }
    if (config.headless || config.test_mode) {
        std::cerr << "Error: --headless/--test are for kamo64-core, not kamo64\n";
        return -1;
    }

    if (!config.log_filepath.empty())
        Utils::set_log_file(config.log_filepath);
    Utils::set_log_level(config.log_level);

    // Cart saves: <app_data_dir>/save/<header image name>/save.sra
    // (same folder as settings.toml; see Ui::app_data_dir)
    N64::g_memory().set_data_dir(N64::Ui::settings_dir_path());

    N64::Ui::App app(config, ui_settings);
    app.run();
    return 0;
}

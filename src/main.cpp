#include "cpu/cpu.h"
#include "memory/memory.h"
#include "memory/pif.h"
#include "mmio/pi.h"
#include "rsp/rsp.h"
#include "spdlog/sinks/basic_file_sink.h"

#include <optional>
#include <spdlog/spdlog.h>

constexpr std::string_view USAGE = "USAGE: n64 <ROM.z64>\n"
                                   "\n"
                                   "optional arguments: \n"
                                   "--log <file>         specify output "
                                   "destination log file output log file\n";

struct CommandArgs {
    std::string rom_path{};
    std::string log_path{};
};

std::optional<CommandArgs> read_args(int argc, char *argv[]) {
    CommandArgs args{};
    if (argc < 2)
        return std::nullopt;

    for (int i = 1; i < argc; ++i) {
        if (const std::string_view read = argv[i]; read == "--log") {
            // ログファイル指定
            if (args.log_path.empty() == false)
                return std::nullopt;
            args.log_path = argv[i + 1];
            i++;
        } else if (read.empty() == false && read[0] != '-') {
            // ROMパス指定
            if (args.rom_path.empty() == false)
                return std::nullopt;
            args.rom_path = read;
        } else {
            return std::nullopt;
        }
    }

    return args;
}

int main(int argc, char *argv[]) {
    const auto args = read_args(argc, argv);
    if (args.has_value() == false || args->rom_path.empty()) {
        std::cout << USAGE << std::endl;
        return -1;
    }

    if (args->log_path.empty() == false) {
        set_default_logger(
            spdlog::basic_logger_mt("basic_logger", args->log_path, true));
    }

    // set logger level
    spdlog::set_level(spdlog::level::trace);
    // use custom logging pattern
    // https://github.com/gabime/spdlog/issues/2073
    spdlog::set_pattern("[%^%l%$] %v");

    N64::g_memory().load_rom(args->rom_path);
    N64::g_memory().reset();
    N64::g_cpu().reset();
    N64::g_rsp().reset();
    N64::g_pi().reset();

    N64::Memory::pif_rom_execute();

    int consumed_cpu_cycles = 0;
    while (true) {
        N64::g_cpu().step();
        consumed_cpu_cycles += N64::Cpu::CPU_CYCLES_PER_INST;

        // RSP ticks 2/3x faster than CPU
        while (consumed_cpu_cycles >= 3) {
            consumed_cpu_cycles -= 3;
            N64::g_rsp().step();
            N64::g_rsp().step();
        }
    }

    return 0;
}
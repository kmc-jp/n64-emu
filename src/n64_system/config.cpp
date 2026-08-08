#include "n64_system/config.h"
#include "utils/log.h"
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace N64 {
namespace N64System {

bool read_config_from_command_line(Config &config, int argc, char *argv[]) {
    if (argc < 2)
        return false;

    // default to info
    config.log_level = Utils::LogLevel::INFO;
    // default to false
    config.test_mode = false;
    config.debug = false;
    config.break_pcs.clear();
    config.watch_paddrs.clear();
    config.break_after_cycles = 0;
#if defined(__x86_64__) || defined(_M_X64)
    config.cpu_backend = CpuBackend::Jit;
#else
    config.cpu_backend = CpuBackend::Interpreter;
#endif
    config.headless = false;
    config.rsp_jit = false;
    config.rsp_thread = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view current = argv[i];
        if (current == "--log") {
            if (config.log_filepath.empty() == false) {
                std::cerr << "Error: log file already specified" << std::endl;
                return false;
            }
            config.log_filepath = argv[i + 1];
            i++;
        } else if (current.starts_with("--log-level=")) {
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
            // Tests only need the CPU; avoid popping a window.
            config.headless = true;
        } else if (current == "--headless") {
            config.headless = true;
        } else if (current == "--debug") {
            config.debug = true;
        } else if (current == "--jit") {
            config.cpu_backend = CpuBackend::Jit;
        } else if (current == "--no-jit") {
            config.cpu_backend = CpuBackend::Interpreter;
        } else if (current == "--rsp-jit") {
            config.rsp_jit = true;
        } else if (current == "--no-rsp-jit") {
            config.rsp_jit = false;
        } else if (current == "--rsp-thread") {
            config.rsp_thread = true;
        } else if (current == "--no-rsp-thread") {
            config.rsp_thread = false;
        } else if (current.starts_with("--break=")) {
            std::string_view pc_str =
                current.substr(std::string("--break=").size());
            char *end = nullptr;
            const std::string pc_s(pc_str);
            const unsigned long pc = std::strtoul(pc_s.c_str(), &end, 0);
            if (end == pc_s.c_str() || *end != '\0') {
                std::cerr << "Error: invalid --break value `" << pc_str << "`"
                          << std::endl;
                return false;
            }
            config.debug = true;
            config.break_pcs.push_back(static_cast<uint32_t>(pc));
        } else if (current.starts_with("--break-after=")) {
            std::string_view n_str =
                current.substr(std::string("--break-after=").size());
            char *end = nullptr;
            const std::string n_s(n_str);
            const unsigned long long n = std::strtoull(n_s.c_str(), &end, 0);
            if (end == n_s.c_str() || *end != '\0' || n == 0) {
                std::cerr << "Error: invalid --break-after value `" << n_str
                          << "`" << std::endl;
                return false;
            }
            config.debug = true;
            config.break_after_cycles = n;
        } else if (current.starts_with("--watch=")) {
            std::string_view p_str =
                current.substr(std::string("--watch=").size());
            char *end = nullptr;
            const std::string p_s(p_str);
            const unsigned long p = std::strtoul(p_s.c_str(), &end, 0);
            if (end == p_s.c_str() || *end != '\0') {
                std::cerr << "Error: invalid --watch value `" << p_str << "`"
                          << std::endl;
                return false;
            }
            config.debug = true;
            config.watch_paddrs.push_back(static_cast<uint32_t>(p));
        } else if (current.empty() == false && !current.starts_with('-')) {
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

#if !defined(__x86_64__) && !defined(_M_X64)
    if (config.cpu_backend == CpuBackend::Jit) {
        std::cerr << "Error: --jit is only supported on x86-64" << std::endl;
        return false;
    }
    if (config.rsp_jit) {
        std::cerr << "Error: --rsp-jit is only supported on x86-64" << std::endl;
        return false;
    }
#endif

    if (config.rsp_thread && config.debug) {
        std::cerr << "Error: --rsp-thread cannot be used with --debug"
                  << std::endl;
        return false;
    }

    return true;
}

} // namespace N64System
} // namespace N64

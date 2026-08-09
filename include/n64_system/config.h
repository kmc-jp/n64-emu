#ifndef CONFIG_H
#define CONFIG_H

#include "utils/log.h"
#include <cstdint>
#include <string>
#include <vector>

namespace N64 {
namespace N64System {

enum class CpuBackend {
    Interpreter,
    Jit,
};

struct Config {
    std::string rom_filepath{};
    std::string log_filepath{};
    Utils::LogLevel log_level;
    bool test_mode;
    bool debug{false};
    std::vector<uint32_t> break_pcs{};
    std::vector<uint32_t> watch_paddrs{};
    // >0: enter debugger when scheduler time reaches this (from boot).
    uint64_t break_after_cycles{0};
#if defined(__x86_64__) || defined(_M_X64)
    CpuBackend cpu_backend{CpuBackend::Jit};
#else
    CpuBackend cpu_backend{CpuBackend::Interpreter};
#endif
    // Run RSP on a worker thread (run-until-halt with quantum). Off by default.
    bool rsp_thread{false};
    // No SDL window / Vulkan present (for CPU tests and CI).
    bool headless{false};
    // Parallel-RDP internal resolution multiplier (1, 2, 4, or 8).
    unsigned upscale{4};
    // Optical-flow frame interpolation (duplicate VI fields -> intermediates).
    bool frame_interp{false};
};

bool read_config_from_command_line(Config &config, int argc, char *argv[]);

} // namespace N64System
} // namespace N64

#endif

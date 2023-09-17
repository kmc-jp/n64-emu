#include "utils/log.h"
#include "cpu/cpu.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

namespace Utils {
constexpr int NUM_BACKTRACE_LOG = 32;

void core_dump() { N64::g_cpu().dump(); }

[[noreturn]] void unimplemented(const std::string what,
                                const std::source_location loc) {
    spdlog::critical("Unimplemented. {}", what);
    spdlog::critical("In `{}` at {}:({},{})", loc.function_name(),
                     loc.file_name(), loc.line(), loc.column());
    spdlog::dump_backtrace();
    core_dump();
    exit(-1);
}

void init_logger() {
    spdlog::set_pattern("[%^%l%$] %v");
    spdlog::enable_backtrace(NUM_BACKTRACE_LOG);
}

void set_log_file(std::string filepath) {
    spdlog::set_default_logger(
        spdlog::basic_logger_mt("basic_logger", filepath, true));
}

void set_log_level(LogLevel level) {
    spdlog::level::level_enum log_level;
    switch (level) {
    case LogLevel::TRACE: {
        log_level = spdlog::level::trace;
    } break;
    case LogLevel::DEBUG: {
        log_level = spdlog::level::debug;
    } break;
    case LogLevel::INFO: {
        log_level = spdlog::level::info;
    } break;
    case LogLevel::CRITICAL: {
        log_level = spdlog::level::critical;
    } break;
    case LogLevel::OFF: {
        log_level = spdlog::level::off;
    } break;
    default: {
        log_level = spdlog::level::off;
    } break;
    }
    spdlog::set_level(log_level);
}

} // namespace Utils

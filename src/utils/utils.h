#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <source_location>
#include <spdlog/spdlog.h>
#include <string>

// PACK
#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__)                                                  \
    __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

namespace Utils {

/* 指定されたポインタから4byte分を読み込む (big endian) */
uint32_t read_from_byte_array32(uint8_t *ptr);

/* 指定されたポインタに4byte分を書き込む (big endian) */
void write_to_byte_array32(uint8_t *ptr, uint32_t value);

void core_dump();

void unimplemented(const std::string what, const std::source_location loc =
                                               std::source_location::current());

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    // WARN,
    // ERROR,
    CRITICAL,
    OFF
};

void init_logger();

void set_log_file(std::string filepath);

void set_log_level(LogLevel level);

template <typename... Args>
inline void debug(fmt::format_string<Args...> fmt, Args &&...args) {
    spdlog::debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void critical(fmt::format_string<Args...> fmt, Args &&...args) {
    spdlog::critical(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void trace(fmt::format_string<Args...> fmt, Args &&...args) {
    spdlog::trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void info(fmt::format_string<Args...> fmt, Args &&...args) {
    spdlog::info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void abort(fmt::format_string<Args...> fmt, Args &&...args) {
    spdlog::critical("Abort!");
    spdlog::critical(fmt, std::forward<Args>(args)...);
    core_dump();
    exit(-1);
}

} // namespace Utils

#endif
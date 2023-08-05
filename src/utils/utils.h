#ifndef UTILS_H
#define UTILS_H

#include <cassert>
#include <cstdint>
#include <source_location>
#include <span>
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

constexpr int NUM_BACKTRACE_LOG = 32;

/* 指定された配列から8byte分を読み込む (big endian) */
inline uint64_t read_from_byte_array64(std::span<const uint8_t> span,
                                       uint64_t offset) {
    assert(offset + 8 <= span.size());
    return (static_cast<uint64_t>(span[offset + 0]) << 56) +
           (static_cast<uint64_t>(span[offset + 1]) << 48) +
           (static_cast<uint64_t>(span[offset + 2]) << 40) +
           (static_cast<uint64_t>(span[offset + 3]) << 32) +
           (static_cast<uint64_t>(span[offset + 4]) << 24) +
           (static_cast<uint64_t>(span[offset + 5]) << 16) +
           (static_cast<uint64_t>(span[offset + 6]) << 8) +
           (static_cast<uint64_t>(span[offset + 7]) << 0);
}

/* 指定された配列から4byte分を読み込む (big endian) */
inline uint32_t read_from_byte_array32(std::span<const uint8_t> span,
                                       uint64_t offset) {
    assert(offset + 4 <= span.size());
    return (span[offset + 0] << 24) + (span[offset + 1] << 16) +
           (span[offset + 2] << 8) + (span[offset + 3] << 0);
}

/* 指定された配列から2byte分を読み込む (big endian) */
inline uint16_t read_from_byte_array16(std::span<const uint8_t> span,
                                       uint64_t offset) {
    assert(offset + 2 <= span.size());
    return (static_cast<uint16_t>(span[offset + 0] << 8)) +
           (static_cast<uint16_t>(span[offset + 1] << 0));
}

/* 指定された配列から1byte分を読み込む (big endian) */
inline uint8_t read_from_byte_array8(std::span<const uint8_t> span,
                                     uint64_t offset) {
    assert(offset + 1 <= span.size());
    return span[offset];
}

/* 指定された配列からWire分を読み込む (big endian) */
template <typename Wire>
inline Wire read_from_byte_array(std::span<const uint8_t> span,
                                 uint64_t offset) {
    static_assert(std::is_same<Wire, uint8_t>::value ||
                  std::is_same<Wire, uint16_t>::value ||
                  std::is_same<Wire, uint32_t>::value ||
                  std::is_same<Wire, uint64_t>::value);

    if constexpr (std::is_same<Wire, uint8_t>::value) {
        return read_from_byte_array8(span, offset);
    } else if constexpr (std::is_same<Wire, uint16_t>::value) {
        return read_from_byte_array16(span, offset);
    } else if constexpr (std::is_same<Wire, uint32_t>::value) {
        return read_from_byte_array32(span, offset);
    } else if constexpr (std::is_same<Wire, uint64_t>::value) {
        return read_from_byte_array64(span, offset);
    } else {
        static_assert(false);
    }
}

/* 指定された配列に8byte分を書き込む (big endian) */
void write_to_byte_array64(std::span<uint8_t> span, uint64_t offset,
                           uint64_t value);

/* 指定された配列に4byte分を書き込む (big endian) */
void write_to_byte_array32(std::span<uint8_t> span, uint64_t offset,
                           uint32_t value);

/* 指定された配列に2byte分を書き込む (big endian) */
void write_to_byte_array16(std::span<uint8_t> span, uint64_t offset,
                           uint16_t value);

/* 指定された配列に1byte分を書き込む (big endian) */
void write_to_byte_array8(std::span<uint8_t> span, uint64_t offset,
                           uint8_t value);

void core_dump();

void unimplemented(const std::string what, const std::source_location loc =
                                               std::source_location::current());

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
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
inline void warn(fmt::format_string<Args...> fmt, Args &&...args) {
    spdlog::warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
[[noreturn]] inline void abort(fmt::format_string<Args...> fmt,
                               Args &&...args) {
    spdlog::critical(fmt, std::forward<Args>(args)...);
    spdlog::dump_backtrace();
    core_dump();
    exit(-1);
}

} // namespace Utils

#endif
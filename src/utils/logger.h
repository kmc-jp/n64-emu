#pragma once

#include <fstream>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/phoenix/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sources/basic_logger.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/value_ref.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/mutable_constant.hpp>

namespace N64 {

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace keywords = boost::log::keywords;

typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;

enum LoggerLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

// 参考: https://oxon.hatenablog.com/entry/2014/12/15/085638

class Logger {
public:
    Logger(const std::string &fileName = "/dev/stderr");

    void SetFilterLevel(LoggerLevel level);
    void Flush();
    void Debug(const std::string &message);
    void Debug(const char *format, ...);
    void Info(const std::string &message);
    void Info(const char *format, ...);
    void Warning(const std::string &message);
    void Warning(const char *format, ...);
    void Error(const std::string &message);
    void Error(const char *format, ...);
    void Fatal(const std::string &message);
    void Fatal(const char *format, ...);

private:
    src::severity_logger_mt<LoggerLevel> logger;
    boost::shared_ptr<text_sink> sink;

    void Log(LoggerLevel level, const std::string &message);
    void Log(LoggerLevel level, const char *format, va_list ap);
};

//extern Logger gLogger;

inline void Logger::Flush() {
    if (sink.get()) {
        sink->flush();
    }
}

inline void Logger::Log(LoggerLevel level, const std::string &message) {
    BOOST_LOG_SEV(logger, level) << message;
}

inline void Logger::Log(LoggerLevel level, const char *format, va_list ap) {
    char str[500];
    vsnprintf(str, 500, format, ap);
    BOOST_LOG_SEV(logger, level) << str;
}

inline void Logger::Debug(const std::string &message) {
    Log(DEBUG, message);
}

inline void Logger::Debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    Log(DEBUG, format, args);
    va_end(args);
}

inline void Logger::Info(const std::string &message) {
    Log(INFO, message);
}

inline void Logger::Info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    Log(INFO, format, args);
    va_end(args);
}

inline void Logger::Warning(const std::string &message) {
    Log(WARNING, message);
}

inline void Logger::Warning(const char *format, ...) {
    va_list args;
    va_start(args, format);
    Log(WARNING, format, args);
    va_end(args);
}

inline void Logger::Error(const std::string &message) {
    Log(ERROR, message);
    Flush();
}

inline void Logger::Error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    Log(ERROR, format, args);
    va_end(args);
    Flush();
}

inline void Logger::Fatal(const std::string &message) {
    Log(FATAL, message);
    Flush();
}

inline void Logger::Fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    Log(FATAL, format, args);
    va_end(args);
    Flush();
}

inline std::ostream &operator<<(std::ostream &strm, LoggerLevel level) {
    static const char *strings[] = {
        "DEBUG", "INFO", "WARNING", "ERROR", "FATAL"
    };

    if (static_cast<std::size_t>(level) < std::size(strings)) {
        strm << strings[level];
    } else {
        strm << static_cast<int>(level);
    }

    return strm;
}

} // N64

#pragma once

#include <string>

namespace pulse::Logger {

enum class LogLevel {
    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

struct LoggerConfig {
    LogLevel default_Logger_level = LogLevel::INFO;
    bool enable_console = false;
    std::string file_path{};
    std::size_t rolling_max_size = 100 * 1024 * 1024;
};

};
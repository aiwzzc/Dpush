#pragma once

#include <format>
#include <source_location>

#include "LoggerManager.h"
#include "Logging.h"

#define LOG_INFO(fmt, ...)  \
    do {                    \
        pulse::Logger::Log::log(pulse::Logger::LogLevel::INFO, \
        std::source_location::current(), \
        fmt, \
        ##__VA_ARGS__);     \
    } while(0)              \

#define LOG_TRACE(fmt, ...) \
    do {                    \
        pulse::Logger::Log::log(pulse::Logger::LogLevel::TRACE, \
        std::source_location::current(), \
        fmt, \
        ##__VA_ARGS__);     \
    } while(0)              \

#define LOG_DEBUG(fmt, ...) \
    do {                    \
        pulse::Logger::Log::log(pulse::Logger::LogLevel::DEBUG, \
        std::source_location::current(), \
        fmt, \
        ##__VA_ARGS__);     \
    } while(0)              \

#define LOG_WARN(fmt, ...)  \
    do {                    \
        pulse::Logger::Log::log(pulse::Logger::LogLevel::WARN, \
        std::source_location::current(), \
        fmt, \
        ##__VA_ARGS__);     \
    } while(0);             \

#define LOG_ERROR(fmt, ...) \
    do {                    \
        pulse::Logger::Log::log(pulse::Logger::LogLevel::ERROR, \
        std::source_location::current(), \
        fmt, \
        ##__VA_ARGS__);     \
    } while(0);             \

#define LOG_FATAL(fmt, ...) \
    do {                    \
        pulse::Logger::Log::log(pulse::Logger::LogLevel::FATAL, \
        std::source_location::current(), \
        fmt, \
        ##__VA_ARGS__);     \
    } while(0);             \

namespace pulse::Logger {

class Log {

public:
    template<typename... Args>
    static void log(LogLevel level, const std::source_location& loc, 
        std::format_string<Args...> fmt, Args&&... args) {

        LoggerManager::Default().log(level, loc, fmt, std::forward<Args>(args)...);
    }
};

};
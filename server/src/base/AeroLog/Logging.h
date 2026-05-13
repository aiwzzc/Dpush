#pragma once

#include <chrono>
#include <format>
#include <source_location>
#include <string>
#include <thread>
#include <memory>
#include <string_view>

#include "AsyncWorker.h"

namespace pulse::Logger {

enum class LogLevel {
    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

class Logger {

public:
    Logger(const std::string& name, LogLevel level, AsyncWorker* worker);

    template<typename... Args>
    void log(LogLevel level, const std::source_location& loc, 
        std::format_string<Args...> fmt, Args&&... args) {

        if(!shouldLog(level)) return;

        std::string payload = std::format(fmt, std::forward<Args>(args)...);

        auto now = std::chrono::system_clock::now();
        auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        auto time = std::format("{:%Y-%m-%d %H:%M:%S}", now);
        const char* level_str = LogLevel2String(level);
        std::string_view file_name = basename(loc.file_name());

        std::string final_log = std::format(
            "[{}] [{}] [tid:{}] [{}:{}] {}\n", time, level_str, 
            tid,
            file_name,
            loc.line(),
            payload
        );

        AppendToThreadLocalBuffer(final_log.data(), final_log.size());
    }

private:
    void AppendToThreadLocalBuffer(const char* data, std::size_t len);
    bool shouldLog(LogLevel level) const;
    const char* LogLevel2String(LogLevel level);
    std::string_view basename(std::string_view path);

private:
    std::string name_;
    LogLevel level_;

    AsyncWorker* worker_;

};

using LoggerPtr = std::shared_ptr<Logger>;

}; // namespace pulse::Logger
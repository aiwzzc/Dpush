#pragma once

#include <chrono>
#include <cstdint>
#include <format>
#include <source_location>
#include <string>
#include <memory>
#include <string_view>
#include <thread>
#include <atomic>

#include "LoggerConfig.h"
#include "AsyncWorker.h"
#include "LogBuffer.h"

namespace pulse::Logger {

constexpr int KSpscEnqueueSpinCount = 10;

class Logger {

public:
    Logger(const std::string& name, LogLevel level, AsyncWorker* worker);

    template<typename... Args>
    void log(
        LogLevel level, 
        const std::source_location& loc, 
        std::format_string<Args...> fmt, 
        Args&&... args
    ) {
        
        if(!shouldLog(level)) return;

        // auto now = std::chrono::system_clock::now();
        // auto time = std::format("{:%Y-%m-%d %H:%M:%S}", now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

        auto tid = GetTid();
        const char* level_str = LogLevel2String(level);
        std::string_view file_name = basename(loc.file_name());

        thread_local auto spsc = this->worker_->RegisterThread();

        LogEntry entry;
        char* out = entry.buffer;
        uint32_t remain = KMaxLogLineSize;

        auto header_res = std::format_to_n(
            out, remain, 
            "[{}] [{}] [tid:{}] [{}:{}] ", 
            now_ms, 
            level_str, 
            tid,
            file_name,
            loc.line()
        );

        std::size_t header_len = std::min<std::size_t>(header_res.size, remain);
        out += header_len;
        remain -= header_len;

        auto payload_res = std::format_to_n(
            out, remain, 
            fmt,
            std::forward<Args>(args)...
        );

        std::size_t payload_len = std::min<std::size_t>(payload_res.size, remain);
        out += payload_len;
        remain -= payload_len;

        if(remain > 0) {
            *out = '\n';
            ++out;
        }

        entry.len += header_len + payload_len + 1;
        // this->worker_->appendMpscBuffers(std::move(entry));
        // this->worker_->notifyBackend();

        if(!spsc->enqueue(std::move(entry))) {
            for(int i = 0 ; i < KSpscEnqueueSpinCount; ++i) {
                if(spsc->enqueue(std::move(entry))) return;
                std::this_thread::yield();
            }
        }
    }

    void setLevel(LogLevel level);
    LogLevel getLevel() const;

private:
    bool shouldLog(LogLevel level) const;
    const char* LogLevel2String(LogLevel level);
    std::string_view basename(std::string_view path);

    inline uint32_t GetTid() {
        thread_local uint32_t tid = static_cast<uint32_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id())
        );

        return tid;
    }

private:
    std::string name_;
    std::atomic<LogLevel> level_;

    AsyncWorker* worker_;

};

using LoggerPtr = std::shared_ptr<Logger>;

}; // namespace pulse::Logger
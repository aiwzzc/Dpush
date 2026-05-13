#include "Logging.h"
#include "LogBuffer.h"

#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>

namespace pulse::Logger {

Logger::Logger(const std::string& name, LogLevel level, AsyncWorker* worker):
name_(name), level_(level), worker_(worker) {}

bool Logger::shouldLog(LogLevel level) const {
    return level >= this->level_;
}

const char* Logger::LogLevel2String(LogLevel level) {
    switch(level) {
        case LogLevel::DEBUG: {
            return "DEBUG";
        }

        case LogLevel::INFO: {
            return "INFO";
        }

        case LogLevel::ERROR: {
            return "ERROR";
        }

        case LogLevel::FATAL: {
            return "FATAL";
        }

        case LogLevel::TRACE: {
            return "TRACE";
        }

        case LogLevel::WARN: {
            return "WARN";
        }

        default:
            return "UNKNOWN";
    }
}

std::string_view Logger::basename(std::string_view path) {
    std::size_t pos = path.find_last_of("/\\");
    if(pos == std::string_view::npos) return path;
    
    return path.substr(pos + 1);
}

void Logger::AppendToThreadLocalBuffer(const char* data, std::size_t len) {
    if(!data || len <= 0) return;

    if(len > KLogBufferSize) return;

    thread_local LogBufferPtr tl_buffer = std::make_unique<LogBuffer>();

    if(tl_buffer->writableBytes() < len) {
        worker_->appendFullBuffers(std::move(tl_buffer));

        tl_buffer = std::make_unique<LogBuffer>();
    }

    tl_buffer->append(data, len);
}

}; // namespace pulse::Logger
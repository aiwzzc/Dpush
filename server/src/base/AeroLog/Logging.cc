#include "Logging.h"

#include <atomic>

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

void Logger::setLevel(LogLevel level) {
    this->level_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::getLevel() const {
    return this->level_.load(std::memory_order_relaxed);
}

}; // namespace pulse::Logger
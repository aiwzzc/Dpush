#pragma once

#include <cstddef>
#include <memory>

namespace pulse::Logger {

inline constexpr std::size_t KTLLogBufferSize = 4 * 1024 * 1024;
inline constexpr std::size_t KMaxLogLineSize = 256;

class LogBuffer {

public:
    std::size_t writableBytes();
    std::size_t length();
    const char* peek() const;
    void append(const char*, std::size_t);
    void retrieveAll();
    char* beginWrite();
    void hasWritten(std::size_t len);

private:
    char buffer_[KTLLogBufferSize];
    char* cur_{buffer_};

};

using LogBufferPtr = std::unique_ptr<LogBuffer>;

struct LogEntry {
    char buffer[KMaxLogLineSize];
    uint32_t len{0};
};

};
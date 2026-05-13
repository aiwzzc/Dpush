#pragma once

#include <vector>
#include <memory>

namespace pulse::Logger {

// inline constexpr std::size_t KLogBufferSize = 4 * 1024 * 1024;
inline constexpr std::size_t KLogBufferSize = 210;

class LogBuffer {

public:
    std::size_t writableBytes();
    std::size_t length();
    const char* peek() const;
    void append(const char*, std::size_t);
    void retrieveAll();

private:
    char buffer_[KLogBufferSize];
    char* cur_{buffer_};

};

using LogBufferPtr = std::unique_ptr<LogBuffer>;

};
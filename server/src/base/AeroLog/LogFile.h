#pragma once

#include <stdio.h>
#include <string>

namespace pulse::Logger {

inline constexpr std::size_t KUserBufferSize = 64 * 1024;

class LogFile {

public:
    explicit LogFile(const std::string& file_path);
    ~LogFile();

    void append(const char* data, std::size_t len);
    void flush();

private:
    ::FILE* fp_;
    char buffer_[KUserBufferSize];
};

};
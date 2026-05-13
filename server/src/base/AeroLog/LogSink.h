#pragma once

#include <cstddef>
#include <memory>

#include "LogFile.h"

namespace pulse::Logger {

class LogSink {

public:
    virtual ~LogSink() = default;

    virtual void append(const char* data, std::size_t len) = 0;

};

using LogSinkPtr = std::shared_ptr<LogSink>;

class FileSink : public LogSink {

public:
    FileSink(const std::string& file_path);

    void append(const char* data, std::size_t len) override;

private:
    std::unique_ptr<LogFile> file_;

};

class ConsoleSink : public LogSink {

public:
    void append(const char* data, std::size_t len) override;

};

};
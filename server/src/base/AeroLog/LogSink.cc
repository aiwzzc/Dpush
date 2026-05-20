#include "LogSink.h"
#include <cstdio>

namespace pulse::Logger {

FileSink::FileSink(const std::string& file_path) {
    this->file_ = std::make_unique<LogFile>(file_path);
}

void FileSink::append(const char* data, std::size_t len) {
    this->file_->append(data, len);
}

void FileSink::flush() {
    this->file_->flush();
}

void ConsoleSink::append(const char* data, std::size_t len) {
    ::fwrite(data, 1, len, stdout);
}

void ConsoleSink::flush() {
    return;
}
    
};
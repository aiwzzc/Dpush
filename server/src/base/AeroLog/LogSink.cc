#include "LogSink.h"

namespace pulse::Logger {

FileSink::FileSink(const std::string& file_path) {
    this->file_ = std::make_unique<LogFile>(file_path);
}

void FileSink::append(const char* data, std::size_t len) {
    this->file_->append(data, len);
}

void ConsoleSink::append(const char* data, std::size_t len) {

}
    
};
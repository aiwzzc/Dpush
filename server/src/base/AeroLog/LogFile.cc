#include "LogFile.h"
#include <stdio.h>
#include <stdexcept>

namespace pulse::Logger {

LogFile::LogFile(const std::string& file_path) {
    this->fp_ = ::fopen(file_path.c_str(), "ae");
    if(!this->fp_) {
        throw std::runtime_error("open log file failed");
    }

    ::setbuffer(this->fp_, this->buffer_, sizeof(this->buffer_));
}

LogFile::~LogFile() {
    if(this->fp_) {
        ::fclose(this->fp_);
    }
}


void LogFile::append(const char* data, std::size_t len) {
    std::size_t written = ::fwrite(data, 1, len, this->fp_);

    if(written != len) {
        throw std::runtime_error("fwrite error");
    }

    this->flush();
}

void LogFile::flush() {
    ::fflush(this->fp_);
}

};
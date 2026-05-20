#include "LogBuffer.h"
#include <string.h>
#include <assert.h>

namespace pulse::Logger {

std::size_t LogBuffer::length() {
    return this->cur_ - this->buffer_;
}

const char* LogBuffer::peek() const 
{ return this->buffer_; }

std::size_t LogBuffer::writableBytes() {
    return KTLLogBufferSize - this->length();
}

void LogBuffer::retrieveAll()
{ this->cur_ = this->buffer_; }

void LogBuffer::append(const char* data, std::size_t size) {
    if(!data || size <= 0) return;

    assert(size <= writableBytes());

    ::memcpy(this->cur_, data, size);
    this->cur_ += size;
}

char* LogBuffer::beginWrite() 
{ return this->cur_; }

void LogBuffer::hasWritten(std::size_t len) 
{ this->cur_ += len; }

};
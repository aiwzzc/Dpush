#include "AsyncWorker.h"
#include "LogBuffer.h"
#include <atomic>
#include <cstring>
#include <optional>
#include <thread>

namespace pulse::Logger {

AsyncWorker::~AsyncWorker() {
    if(this->running_) {
        this->stop();
    }
}

void AsyncWorker::appendMpscBuffers(LogEntry&& entry) {
    while(!this->mpsc_.enqueue(std::move(entry))) {
        std::this_thread::yield();
    }
}

void AsyncWorker::start() {
    this->backend_thread_ = std::thread([this] () {
        this->backendThreadFunc();
    });
}

void AsyncWorker::stop() {
    this->running_ = false;
    this->notifyBackend();

    if(this->backend_thread_.joinable()) {
        this->backend_thread_.join();
    }
}

void AsyncWorker::addSink(LogSinkPtr LogSink) {
    if(LogSink) {
        this->sinks_.emplace_back(std::move(LogSink));
    }
}

void AsyncWorker::notifyBackend() {
    this->pending_counts_.fetch_add(1, std::memory_order_release);
    this->pending_counts_.notify_one();
}

void AsyncWorker::backendThreadFunc() {
    char batch_buffer[KBatchBufferSize];
    char* out = batch_buffer;
    std::size_t remain = KBatchBufferSize;

    while(this->running_) {
#if 0
        int current = this->pending_counts_.load(std::memory_order_acquire);
        if(current == 0) {
            this->pending_counts_.wait(0, std::memory_order_acquire);
        }
#endif
        std::optional<LogEntry> buffer_opt;
        int processed{0};

        while((buffer_opt = this->mpsc_.dequeue())) {
            LogEntry& entry = buffer_opt.value();

            if(entry.len > remain) {
                for(const auto& sink : this->sinks_) {
                    sink->append(batch_buffer, KBatchBufferSize - remain);
                }

                out = batch_buffer;
                remain = KBatchBufferSize;
            }

            ::memcpy(out, entry.buffer, entry.len);
            out += entry.len;
            remain -= entry.len;

            ++processed;
        }

        if(processed > 0) {
            std::size_t written = KBatchBufferSize - remain;
            if(written > 0) {
                for(const auto& sink : this->sinks_) {
                    sink->append(batch_buffer, written);
                }
            }

            for(const auto& sink : this->sinks_) {
                sink->flush();
            }

            // this->pending_counts_.fetch_sub(processed, std::memory_order_release);
        }
    }
}
    
};
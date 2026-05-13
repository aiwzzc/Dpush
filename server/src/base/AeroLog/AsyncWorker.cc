#include "AsyncWorker.h"

namespace pulse::Logger {

void AsyncWorker::appendFullBuffers(LogBufferPtr tl_buffer) {
    std::lock_guard<std::mutex> lock(this->g_mutex_);
    this->g_full_buffers_.emplace_back(std::move(tl_buffer));
    this->g_cond_.notify_one();
}

void AsyncWorker::start() {
    this->backend_thread_ = std::thread([this] () {
        this->backendThreadFunc();
    });
}

void AsyncWorker::stop() {
    this->running_ = false;

    if(this->backend_thread_.joinable()) {
        this->backend_thread_.join();
    }
}

void AsyncWorker::addSink(LogSinkPtr LogSink) {
    if(LogSink) {
        this->sinks_.emplace_back(std::move(LogSink));
    }
}

void AsyncWorker::backendThreadFunc() {
    std::vector<LogBufferPtr> write_buffers;

    while(this->running_) {
        {
            std::unique_lock<std::mutex> lock(this->g_mutex_);
            this->g_cond_.wait(lock, [this] () {
                return !this->g_full_buffers_.empty() || !this->running_;
            });

            write_buffers.swap(this->g_full_buffers_);
        }

        for(const auto& buffer : write_buffers) {
            for(const auto& sink : this->sinks_) {
                sink->append(buffer->peek(), buffer->length());   
            }
        }

        //this->log_file_.flush();
        write_buffers.clear();
    }
}
    
};
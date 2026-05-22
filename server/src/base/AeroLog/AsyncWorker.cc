#include "AsyncWorker.h"
#include "LogBuffer.h"
#include "SPSC.hpp"
#include <atomic>
#include <cstring>
#include <mutex>
#include <optional>
#include <thread>
#include <algorithm>

namespace pulse::Logger {

AsyncWorker::AsyncWorker() {
    this->spsc_array_.store(new SpscRingBufferArray(0), std::memory_order_relaxed);
}

AsyncWorker::~AsyncWorker() {
    this->stop();
}

#if 0
void AsyncWorker::appendMpscBuffers(LogEntry&& entry) {
    while(!this->mpsc_.enqueue(std::move(entry))) {
        std::this_thread::yield();
    }
}
#endif

void AsyncWorker::start() {
    this->backend_thread_ = std::thread([this] () {
        this->backendThreadFunc();
    });
}

void AsyncWorker::stop() {
    SpscRingBufferArrayPtr spsc_array = this->spsc_array_.load(std::memory_order_acquire);
    if(spsc_array) spsc_array->release();

    if(this->running_) {
        this->running_ = false;
        // this->notifyBackend();

        if(this->backend_thread_.joinable()) {
            this->backend_thread_.join();
        }
    }
}

void AsyncWorker::addSink(LogSinkPtr LogSink) {
    if(LogSink) {
        this->sinks_.emplace_back(std::move(LogSink));
    }
}

#if 0
void AsyncWorker::notifyBackend() {
    this->pending_counts_.fetch_add(1, std::memory_order_release);
    this->pending_counts_.notify_one();
}
#endif

AsyncWorker::tl_SpscRingBufferPtr AsyncWorker::RegisterThread() {
    auto spsc = std::make_shared<SpscRingBuffer<LogEntry, KSpscRingBufferCapacity>>();

    std::lock_guard<std::mutex> lock(this->register_mutex_);

    SpscRingBufferArrayPtr old_array = this->spsc_array_.load(std::memory_order_acquire);
    std::size_t new_count = old_array->count_ + 1;
    SpscRingBufferArrayPtr new_array = new SpscRingBufferArray(new_count);

    if(old_array->count_ > 0) {
        std::copy(old_array->array_, old_array->array_ + old_array->count_, new_array->array_);
    }

    new_array->array_[old_array->count_] = spsc.get();
    this->spsc_array_.store(new_array, std::memory_order_release);
    old_array->release();

    this->owned_queues_.emplace_back(spsc);
    return spsc;
}

void AsyncWorker::backendThreadFunc() {
    char batch_buffer[KBatchBufferSize];
    char* out = batch_buffer;
    std::size_t remain = KBatchBufferSize;
    std::size_t round_robin_idx{0};

    while(this->running_) {
#if 0
        int current = this->pending_counts_.load(std::memory_order_acquire);
        if(current == 0) {
            this->pending_counts_.wait(0, std::memory_order_acquire);
        }
#elif 0
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
#endif
        std::optional<LogEntry> buffer_opt;
        int processed{0};

        SpscRingBufferArrayPtr spsc_array = this->spsc_array_.load(std::memory_order_acquire);
        spsc_array->acquire();

        if(spsc_array->count_ == 0) {
            spsc_array->release();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        for(std::size_t i = 0; i < spsc_array->count_; ++i) {
            std::size_t index = (round_robin_idx + i) % spsc_array->count_;
            SpscRingBufferPtr spsc_ptr = spsc_array->array_[index];

            int batch_count{0};
            while(batch_count < KMaxPerQueueSize && (buffer_opt = spsc_ptr->dequeue())) {
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
                ++batch_count;
            }
        }

        round_robin_idx = (round_robin_idx + 1) % spsc_array->count_;
        spsc_array->release();

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

        } else {
            std::this_thread::yield();
        }
    }
}
    
};
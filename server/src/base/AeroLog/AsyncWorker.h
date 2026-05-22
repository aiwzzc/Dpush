#pragma once

#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

#include "LogBuffer.h"
#include "LogSink.h"
#if 0
#include "MPSC.hpp"
#endif
#include "SPSC.hpp"

namespace pulse::Logger {

inline constexpr std::size_t KMpscRingBufferCapacity = 65536;
inline constexpr std::size_t KSpscRingBufferCapacity = 65536;
inline constexpr std::size_t KBatchBufferSize = 4 * 1024 * 1024;

constexpr int KMaxPerQueueSize = 128;

class AsyncWorker {

private:
    using SpscRingBufferPtr = SpscRingBuffer<LogEntry, KSpscRingBufferCapacity>*;
    using SpscQueueArrayPtr = SpscRingBufferPtr*;

    struct SpscRingBufferArray {

        SpscRingBufferArray(std::size_t n) : count_(n) {
            this->array_ = new SpscRingBufferPtr[n];
        }

        ~SpscRingBufferArray() {
            delete[] this->array_;
        }

        void acquire() {
            this->ref_count_.fetch_add(1, std::memory_order_release);
        }

        void release() {
            if(this->ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete this;
            }
        }

        SpscQueueArrayPtr array_;
        std::size_t count_{0};
        std::atomic<int> ref_count_{1};
    };

private:
    using tl_SpscRingBufferPtr = std::shared_ptr<SpscRingBuffer<LogEntry, KSpscRingBufferCapacity>>;
    using SpscRingBufferArrayPtr = SpscRingBufferArray*;

public:
    AsyncWorker();
    ~AsyncWorker();
#if 0
    void appendMpscBuffers(LogEntry&& entry);
    void notifyBackend();
#endif
    void start();
    void stop();
    void addSink(LogSinkPtr LogSink);

    tl_SpscRingBufferPtr RegisterThread();

private:
    void backendThreadFunc();

private:
    std::vector<LogSinkPtr> sinks_;
#if 0
    MpscRingBuffer<LogEntry, KMpscRingBufferCapacity> mpsc_;
    std::atomic<int> pending_counts_{0};
#endif
    std::thread backend_thread_;
    bool running_{true};

    std::atomic<SpscRingBufferArrayPtr> spsc_array_{nullptr};
    std::mutex register_mutex_;
    std::vector<tl_SpscRingBufferPtr> owned_queues_;
};

using AsyncWorkerPtr = std::unique_ptr<AsyncWorker>;

};
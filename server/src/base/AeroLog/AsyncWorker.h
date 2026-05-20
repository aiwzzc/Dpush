#pragma once

#include <thread>
#include <vector>
#include <memory>
#include <atomic>

#include "LogBuffer.h"
#include "LogSink.h"
#include "MPSC.hpp"

namespace pulse::Logger {

inline constexpr std::size_t KMpscRingBufferCapacity = 65536;
inline constexpr std::size_t KBatchBufferSize = 4 * 1024 * 1024;

class AsyncWorker {

public:
    ~AsyncWorker();

    void appendMpscBuffers(LogEntry&& entry);
    void notifyBackend();

    void start();
    void stop();
    void addSink(LogSinkPtr LogSink);

private:
    void backendThreadFunc();

private:
    std::vector<LogSinkPtr> sinks_;

    MpscRingBuffer<LogEntry, KMpscRingBufferCapacity> mpsc_;
    std::atomic<int> pending_counts_{0};

    std::thread backend_thread_;
    bool running_{true};

};

using AsyncWorkerPtr = std::unique_ptr<AsyncWorker>;

};
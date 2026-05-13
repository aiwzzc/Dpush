#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <memory>

#include "LogBuffer.h"
#include "LogSink.h"

namespace pulse::Logger {

class AsyncWorker {

public:
    void appendFullBuffers(LogBufferPtr tl_buffer);

    void start();
    void stop();
    void addSink(LogSinkPtr LogSink);

private:
    void backendThreadFunc();

private:
    std::vector<LogSinkPtr> sinks_;

    std::mutex g_mutex_;
    std::condition_variable g_cond_;
    std::vector<LogBufferPtr> g_full_buffers_;
    std::thread backend_thread_;

    bool running_{true};

};

using AsyncWorkerPtr = std::unique_ptr<AsyncWorker>;

};
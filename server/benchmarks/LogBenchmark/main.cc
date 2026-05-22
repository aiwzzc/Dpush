#include "LogApi.h"
#include "LoggerManager.h"
#include "LogSink.h"

#include <memory>
#include <benchmark/benchmark.h>

using namespace pulse::Logger;

constexpr int thread_count = 4;
constexpr std::size_t Log_count_per_thread = 50000;

// ============================================================================
// 1. 前端纯延迟测试（无后端，测 enqueue 开销）
// ============================================================================
class NullSink : public LogSink {
public:
    void append(const char*, std::size_t) override {}
    void flush() override {}
};

static void BM_PureFrontendLatency(benchmark::State& state) {
    // 确保线程在压测前注册好 SPSC
    LoggerManager::Instance(); 

    for(auto _ : state) {
        // 每次只打一条！Google Benchmark 会自动统计单次耗时
        // LOG_INFO("thread begin log: {}", 42); 
        LOG_INFO("fixed");
    }
}

// 4 线程并发压测
BENCHMARK(BM_PureFrontendLatency) -> Threads(thread_count);

int main(int argc, char** argv) {

    auto null_sink = std::make_shared<NullSink>();
    LoggerManager::Instance().addSink(null_sink);
    LoggerManager::Instance().start();

    // auto file_sink = std::make_shared<FileSink>("../logs/benchmark.log");
    // LoggerManager::Instance().addSink(file_sink);
    // LoggerManager::Instance().start();

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();

    return 0;
}
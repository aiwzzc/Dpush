#include "LogApi.h"
#include "LoggerManager.h"
#include "LogSink.h"

#include <memory>
#include <benchmark/benchmark.h>

using namespace pulse::Logger;

constexpr int thread_count = 4;
constexpr std::size_t Log_count_per_thread = 10000;

static void BM_LoggerThroughput(benchmark::State& state) {
    for(auto _ : state) {
        for(int i = 0; i < Log_count_per_thread; ++i) {
            LOG_INFO("{}", "thread begin log");
        }
    }

    state.SetItemsProcessed(
        state.iterations() * 
        Log_count_per_thread
    );
}

BENCHMARK(BM_LoggerThroughput) -> Threads(thread_count);

int main(int argc, char** argv) {

    auto file_sink = std::make_shared<FileSink>("../logs/benchmark.log");
    LoggerManager::Instance().addSink(file_sink);
    LoggerManager::Instance().start();

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();

    return 0;
}
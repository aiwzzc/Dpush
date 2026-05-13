#include "LogApi.h"
#include "LoggerManager.h"
#include "LogSink.h"

#include <memory>

using namespace pulse::Logger;

int main() {

    auto file_sink = std::make_shared<FileSink>("../logs/benchmark.log");
    LoggerManager::Instance().addSink(file_sink);
    LoggerManager::Instance().start();

    LOG_INFO("{}", "hello world");

    return 0;
}
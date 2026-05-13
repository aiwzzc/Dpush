#include "DispatchServer.h"
#include "DispatchConfig.h"
#include "RootConfig/RootConfigLoader.h"
#include "ConfigBuilder/ConfigBuilder.h"

#include "AeroLog/LoggerManager.h"
#include "AeroLog/LogApi.h"
#include "AeroLog/LogSink.h"

using namespace pulse::config;
using namespace pulse::Logger;

int main(int argc, char* argv[]) {

    if(argc < 3) return -1;

    auto file_sink = std::make_shared<FileSink>("../logs/app.log");
    LoggerManager::Instance().addSink(file_sink);
    LoggerManager::Instance().start();

    ConfigBuilder configBuilder(argc, argv);
    DispatchConfig config = configBuilder.Build<RootConfig, DispatchConfig>(
        RootConfigLoader::LoadRoot,
        DispatchConfigLoader::LoadDispatch
    );

    LOG_INFO("{}", "Dispatch Server start success 1");
    LOG_ERROR("{}", "Dispatch Server start success 2");
    LOG_FATAL("{}", "Dispatch Server start success 3");
    LOG_WARN("{}", "Dispatch Server start success 4");

    dispatchServer server{config};

    server.start();

    return 0;
}
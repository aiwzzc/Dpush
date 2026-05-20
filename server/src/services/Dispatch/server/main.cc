#include "DispatchServer.h"
#include "DispatchConfig.h"
#include "RootConfig/RootConfigLoader.h"
#include "ConfigBuilder/ConfigBuilder.h"

#include "AeroLog/LoggerManager.h"
#include "AeroLog/LogApi.h"
#include "AeroLog/LoggerConfig.h"

using namespace pulse::config;
using namespace pulse::Logger;

int main(int argc, char* argv[]) {

    if(argc < 3) return -1;

    LoggerManager::Instance().start({
        LogLevel::INFO, 
        true, 
        "../logs/app.log"
    });

    ConfigBuilder configBuilder(argc, argv);
    DispatchConfig config = configBuilder.Build<RootConfig, DispatchConfig>(
        RootConfigLoader::LoadRoot,
        DispatchConfigLoader::LoadDispatch
    );

    LOG_INFO("{}", "Dispatch Server Start Success");

    dispatchServer server{config};

    server.start();

    return 0;
}
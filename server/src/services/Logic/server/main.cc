#include "LogicServer.h"
#include "RootConfig/RootConfigLoader.h"
#include "ConfigBuilder/ConfigBuilder.h"
#include "LogicConfig.h"

#include "AeroLog/LoggerManager.h"
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

    ConfigBuilder builder(argc, argv);
    pulse::config::LogicConfig config = builder.Build<RootConfig, pulse::config::LogicConfig>(
        RootConfigLoader::LoadRoot,
        LogicConfigLoader::LoadLogic
    );

    LogicServer server{config};
    server.start();

    return 0;
}
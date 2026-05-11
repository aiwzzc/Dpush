#include "LogicServer.h"
#include "RootConfig/RootConfigLoader.h"
#include "ConfigBuilder/ConfigBuilder.h"
#include "LogicConfig.h"

using namespace pulse::config;

int main(int argc, char* argv[]) {

    if(argc < 3) return -1;

    ConfigBuilder builder(argc, argv);
    pulse::config::LogicConfig config = builder.Build<RootConfig, pulse::config::LogicConfig>(
        RootConfigLoader::LoadRoot,
        LogicConfigLoader::LoadLogic
    );

    LogicServer server{config};
    server.start();

    return 0;
}
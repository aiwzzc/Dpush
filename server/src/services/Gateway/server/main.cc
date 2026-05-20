#include "GatewayServer.h"
#include "muduo/base/Logging.h"
#include "GatewayConfig.h"

#include "RootConfig/RootConfigLoader.h"
#include "ConfigBuilder/ConfigBuilder.h"

using namespace pulse::config;

int main(int argc, char* argv[]) {

    if(argc < 3) return -1;

    ConfigBuilder builder(argc, argv);
    GatewayConfig config = builder.Build<RootConfig, GatewayConfig>(
        RootConfigLoader::LoadRoot,
        GatewayConfigLoader::LoadGateway
    );
    
    muduo::Logger::setLogLevel(muduo::Logger::FATAL);

    GatewayServer gatewayServer{config};

    gatewayServer.start();

    return 0;
}
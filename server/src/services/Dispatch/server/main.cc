#include "DispatchServer.h"
#include "DispatchConfig.h"
#include "RootConfig/RootConfigLoader.h"
#include "ConfigBuilder/ConfigBuilder.h"

using namespace pulse::config;

int main(int argc, char* argv[]) {

    if(argc < 3) return -1;

    ConfigBuilder configBuilder(argc, argv);
    DispatchConfig config = configBuilder.Build<RootConfig, DispatchConfig>(
        RootConfigLoader::LoadRoot,
        DispatchConfigLoader::LoadDispatch
    );

    dispatchServer server{config};

    server.start();

    return 0;
}
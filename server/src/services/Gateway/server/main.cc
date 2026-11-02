#include "GatewayServer.h"
#include "muduo/base/Logging.h"
#include "GatewayConfig.h"

#include "RootConfig/RootConfigLoader.h"
#include "ConfigBuilder/ConfigBuilder.h"
#include <cstdint>

#if 1

#include <atomic>
#include <thread>
#include "AeroLog/LoggerManager.h"
#include "AeroLog/LogApi.h"

    std::atomic<int> writed_conns{0};
    std::atomic<uint64_t> start_latency{0};
    std::atomic<uint64_t> end_latency{0};

#endif

using namespace pulse::config;
using namespace pulse::Logger;

int main(int argc, char* argv[]) {

    if(argc < 3) return -1;

    ConfigBuilder builder(argc, argv);
    GatewayConfig config = builder.Build<RootConfig, GatewayConfig>(
        RootConfigLoader::LoadRoot,
        GatewayConfigLoader::LoadGateway
    );
    
    muduo::Logger::setLogLevel(muduo::Logger::FATAL);
    LoggerManager::Instance().start();

#if 1

    std::thread([] () {

        sleep(60 * 3);

        while(writed_conns.load(std::memory_order_acquire) < 100000) {
            sleep(1);
        }

        uint64_t latency = end_latency.load(std::memory_order_acquire) - start_latency.load(std::memory_order_acquire);

        LOG_INFO("latency: {}", latency);

    }).detach();

#endif

    GatewayServer gatewayServer{config};

    gatewayServer.start();

    return 0;
}
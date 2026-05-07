#include "GatewayServer.h"
#include "muduo/base/Logging.h"
#include "config.h"

int main(int argc, char* argv[]) {

    if(argc < 2) return -1;

    Config& config = Config::getInstance();
    config.Parser(argv[1]);

    muduo::Logger::setLogLevel(muduo::Logger::FATAL);

    GatewayServer gatewayServer;

    gatewayServer.start();

    return 0;
}
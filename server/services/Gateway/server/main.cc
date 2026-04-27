#include "GatewayServer.h"
#include "muduo/base/Logging.h"

int main() {

    muduo::Logger::setLogLevel(muduo::Logger::FATAL);

    GatewayServer gatewayServer;

    gatewayServer.start();

    return 0;
}
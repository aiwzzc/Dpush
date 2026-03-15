#include "GatewayServer.h"

int main() {

    // HttpServer httpserver{2};
    // grpcClientPtr grpcclient = std::make_shared<grpcClient>();

    // GatewayPubSubManager pubsubManager;

    // kafkaProducer producer;

    // httpserver.setHttpCallback([grpcclient] (TcpConnectionPtr conn, HttpRequest req) {
    //     handleHttpEvent(conn, req, grpcclient);
    // });

    // httpserver.setUpgradeCallback([grpcclient, &producer] (const TcpConnectionPtr& conn, const HttpRequest& req) {
    //     handleUpgradeEvent(conn, req, grpcclient);
    // });

    // httpserver.start();

    GatewayServer gatewayServer;

    gatewayServer.start();

    return 0;
}
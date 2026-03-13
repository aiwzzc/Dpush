#include "muduo/http/HttpServer.h"
#include "muduo/http/HttpRequest.h"
#include "handleHttpEvent.h"
#include "handleUpgradeEvent.h"
#include "grpcClient.h"
#include "GatewayPubSubManager.h"

int main() {

    HttpServer httpserver{2};
    grpcClientPtr grpcclient = std::make_shared<grpcClient>();

    GatewayPubSubManager pubsubManager;

    httpserver.setHttpCallback([grpcclient] (TcpConnectionPtr conn, HttpRequest req) {
        handleHttpEvent(conn, req, grpcclient);
    });

    httpserver.setUpgradeCallback([grpcclient] (const TcpConnectionPtr& conn, const HttpRequest& req) {
        handleUpgradeEvent(conn, req, grpcclient);
    });

    httpserver.start();

    return 0;
}
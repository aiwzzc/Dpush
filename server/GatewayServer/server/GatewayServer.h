#pragma once

#include <memory>
#include <thread>



#include "httpServer/HttpServer.h"
#include "httpServer/HttpRequest.h"
#include "handleHttpEvent.h"
#include "handleUpgradeEvent.h"
#include "grpcClient.h"
#include "GatewayPubSubManager.h"
#include "producer.h"

class GatewayServer {

public:
    GatewayServer();
    ~GatewayServer();

    void start();

private:
    std::unique_ptr<HttpServer> HttpServer_;
    std::shared_ptr<grpcClient> grpcClient_;
    std::unique_ptr<GatewayPubSubManager> GatewayPubSubManager_;
    std::shared_ptr<kafkaProducer> kafkaProducer_;
    std::thread poolthread_;
};
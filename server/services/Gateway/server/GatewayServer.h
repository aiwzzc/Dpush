#pragma once

#include <memory>
#include <thread>

#include "HttpServer.h"
#include "HttpRequest.h"
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

    static const char* public_key;

private:
    static constexpr long long MAX_CONN_SIZE = 100000;

    std::unique_ptr<HttpServer> HttpServer_;
    std::shared_ptr<grpcClient> grpcClient_;
    std::unique_ptr<GatewayPubSubManager> GatewayPubSubManager_;
    std::shared_ptr<kafkaProducer> kafkaProducer_;
    std::thread poolthread_;
};
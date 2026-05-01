#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>

#include "HttpServer.h"
#include "HttpRequest.h"
#include "handleHttpEvent.h"
#include "handleUpgradeEvent.h"
#include "grpcClient.h"
#include "GatewayPubSubManager.h"
#include "producer.h"
#include "grpcServer.h"
#include "GatewayRegister.h"

namespace muduo {
namespace net {
    class EventLoop;
};
};

class GatewayServer {

public:
    GatewayServer();
    ~GatewayServer();

    void start();

    static const char* public_key;
    static std::unordered_map<int32_t, muduo::net::EventLoop*> user_Eventloop_;
    static std::shared_mutex user_Eventloop_mutex_;

private:
    static constexpr long long MAX_CONN_SIZE = 100000;

    std::unique_ptr<HttpServer> HttpServer_;
    std::shared_ptr<grpcClient> grpcClient_;
    std::unique_ptr<GatewayPubSubManager> GatewayPubSubManager_;
    std::shared_ptr<kafkaProducer> kafkaProducer_;
    std::thread poolthread_;

    std::unique_ptr<GatewayGrpcServer> grpcServer_;
    std::unique_ptr<GatewayRegister> register_;
};
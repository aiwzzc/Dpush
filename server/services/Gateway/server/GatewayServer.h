#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>

#include <sw/redis++/redis++.h>

#include "httpServer/HttpServer.h"
#include "httpServer/HttpRequest.h"
#include "handleHttpEvent.h"
#include "handleUpgradeEvent.h"
#include "grpcClient.h"
#include "GatewayPubSubManager.h"
#include "producer.h"
#include "grpcServer.h"
#include "GatewayRegister.h"
#include "concurrentqueue/concurrentqueue.h"
#include "types.h"

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
    static std::atomic<long> conned_count_;

private:
    static constexpr long MAX_CONN_SIZE = 100000;

    void collect_load_to_spsc();
    void write_load_to_redis(GatewayLoad& load);

    std::unique_ptr<HttpServer> HttpServer_;
    std::shared_ptr<grpcClient> grpcClient_;
    std::unique_ptr<GatewayPubSubManager> GatewayPubSubManager_;
    std::shared_ptr<kafkaProducer> kafkaProducer_;
    bool running_{true};
    std::thread poolthread_;
    std::thread redis_worker_;
    std::unique_ptr<sw::redis::Redis> redisPool_;

    std::unique_ptr<moodycamel::ConcurrentQueue<GatewayLoad>> load_queue_;

    std::unique_ptr<GatewayGrpcServer> grpcServer_;
    std::unique_ptr<GatewayRegister> register_;
};
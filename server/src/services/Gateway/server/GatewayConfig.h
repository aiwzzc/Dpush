#pragma once

#include <string>
#include "RootConfig/RootConfig.h"

class grpcClient;
class kafkaProducer;

namespace pulse::config {

struct GatewayService {
    std::string name;
    std::string instance_id;
    std::string host;
    int port{5005};
    int io_thread_count;
    int weight{1};
};

struct GatewayConfig {
    GatewayService service;
    RootConfig infra;
    std::string endpoint;

    // gateway need watch serviced prefix
    struct ServiceRegistry {
        std::string logic_prefix_;
        std::string room_prefix_;
    };

    ServiceRegistry serviceRegistry;
};

struct WsServerContext {
    grpcClient* grpcClient_;
    kafkaProducer* producer_;
    const std::string& endpoint_;
};

class GatewayConfigLoader {

public:
    static GatewayConfig LoadGateway(const std::string& path, RootConfig&& rootConfig);

};

};
#pragma once

#include "RootConfig/RootConfig.h"

namespace pulse::config {

struct DispatchService {
    std::string name;
    std::string instance_id;
    std::string host;
    int port{5001};
    int io_thread_count;
    int weight{1};
};

struct DispatchConfig {
    DispatchService service;
    RootConfig infra;
    std::string endpoint;

    struct ServiceRegistry {
        std::string gateway_prefix_;
        std::string auth_prefix_;
        std::string logic_prefix_;
    };

    ServiceRegistry serviceRegistry;
};

class DispatchConfigLoader {

public:
    static DispatchConfig LoadDispatch(const std::string& path, RootConfig&& rootConfig);

};

};
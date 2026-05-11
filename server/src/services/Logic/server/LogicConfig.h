#pragma once

#include <string>
#include <vector>
#include "RootConfig/RootConfig.h"

namespace pulse::config {

struct LogicServiceConfig {
    std::string name;
    std::string instance_id;
    std::string host;
    int port{5008};
    int weight{1};
};

struct ConcurrencyConfig {
    int thread_pool_size{6};
};

struct KafkaConsumerConfig {
    std::vector<std::string> topics;
};

struct LogicConfig {
    LogicServiceConfig service;
    ConcurrencyConfig concurrency;
    RootConfig infra;
    std::string endpoint;

    struct ServiceRegistry {
        std::string gateway_prefix_;
    };

    ServiceRegistry serviceRegistry;

    KafkaConsumerConfig kafkaConsumerConfig;
};

class LogicConfigLoader {

public:
    static LogicConfig LoadLogic(const std::string& path, RootConfig&& rootConfig);

};
    
};
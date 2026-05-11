#include "LogicConfig.h"
#include <yaml-cpp/yaml.h>

namespace pulse::config {

LogicConfig LogicConfigLoader::LoadLogic(const std::string& path, RootConfig&& rootConfig) {
    YAML::Node root = YAML::LoadFile(path);
    
    LogicConfig config;
    auto service = root["service"];

    if(service) {
        auto name = service["name"];
        if(name) config.service.name = name.as<std::string>();

        auto instance_id = service["instance_id"];
        if(instance_id) config.service.instance_id = instance_id.as<std::string>();

        auto host = service["host"];
        if(host) config.service.host = host.as<std::string>();

        auto port = service["port"];
        if(port) config.service.port = port.as<int>();

        auto weight = service["weight"];
        if(weight) config.service.weight = weight.as<int>();

        config.endpoint = config.service.host + ":" + std::to_string(config.service.port);
    }

    auto concurrency = root["concurrency"];
    if(concurrency) {
        auto thread_pool_size = concurrency["thread_pool_size"];
        if(thread_pool_size) config.concurrency.thread_pool_size = thread_pool_size.as<int>();
    }

    auto kafka_consumer = root["kafka_consumer"];
    if(kafka_consumer) {
        auto topics = kafka_consumer["topics"];
        if(topics) {
            auto topics_vec = topics.as<std::vector<std::string>>();
            for(const auto& topic : topics_vec) {
                config.kafkaConsumerConfig.topics.emplace_back(topic);
            }
        }
    }
    
    config.serviceRegistry.gateway_prefix_ = rootConfig.serviceRegistryConfig.gateway_service_prefix;
    config.infra = std::move(rootConfig);

    return config;
}

};
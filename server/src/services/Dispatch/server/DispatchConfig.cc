#include "DispatchConfig.h"
#include <yaml-cpp/yaml.h>

namespace pulse::config {

DispatchConfig DispatchConfigLoader::LoadDispatch(const std::string& path, RootConfig&& rootConfig) {
    YAML::Node service = YAML::LoadFile(path);
    auto root = service["service"];

    DispatchConfig config;

    auto name = root["name"];
    if(name) config.service.name = name.as<std::string>();

    auto instance_id = root["instance_id"];
    if(instance_id) config.service.instance_id = instance_id.as<std::string>();

    auto host = root["host"];
    if(host) config.service.host = host.as<std::string>();

    auto port = root["port"];
    if(port) config.service.port = port.as<int>();

    auto io_thread_count = root["io_thread_count"];
    if(io_thread_count) config.service.io_thread_count = io_thread_count.as<int>();

    auto weight = root["weight"];
    if(weight) config.service.weight = weight.as<int>();

    config.endpoint = config.service.host + ":" + std::to_string(config.service.port);
    
    config.serviceRegistry.logic_prefix_ = rootConfig.serviceRegistryConfig.logic_service_prefix;
    config.serviceRegistry.gateway_prefix_ = rootConfig.serviceRegistryConfig.gateway_service_prefix;
    config.serviceRegistry.auth_prefix_ = rootConfig.serviceRegistryConfig.auth_service_prefix;

    config.infra = std::move(rootConfig);

    return config;
}

};
#pragma once

#include <string>
#include "RootConfig.h"

#include <yaml-cpp/yaml.h>

namespace pulse::config {

class RootConfigLoader {

public:
    static RootConfig LoadRoot(const std::string& path);

private:

    static void LoadRootEtcd(YAML::Node& root, EtcdConfig& config);
    static void LoadRootRedis(YAML::Node& root, RedisConfig& config);
    static void LoadRootMysql(YAML::Node& root, MysqlConfig& config);
    static void LoadRootKafka(YAML::Node& root, KafkaConfig& config);
    static void LoadRootLog(YAML::Node& root, LogConfig config);
    static void LoadRootServiceRegistry(YAML::Node& root, ServiceRegistryConfig& config);
};

};
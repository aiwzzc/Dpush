#pragma once

#include <string>
#include <vector>

namespace pulse::config {

struct EndpointConfig {
    std::string host;
    int port;
};

struct EtcdConfig {
    std::vector<std::string> endpoints;
    int timeout_ms{3000};
};

struct RedisConfig {
    std::vector<EndpointConfig> endpoints;
    int db{1};
    int pool_size{16};
    int timeout_ms{2000};
};

struct MysqlConfig {
    std::vector<EndpointConfig> endpoints;
    std::string username;
    std::string password;
    std::string database;
    int pool_thread_count{4};
    int pool_size{16};
    int timeout_ms{5000};
};

struct KafkaConfig {
    std::vector<std::string> endpoints;
    
    struct Producer {
        std::string acks;
        std::string enable_idempotence;
    };

    struct Consumer {
        std::string group_id;
        std::string auto_offset_reset;
        std::string enable_auto_commit;
        std::string enable_auto_offset_store;
    };

    Producer producer;
    Consumer consumer;
};

struct LogConfig {
    std::string level;
};

struct ServiceRegistryConfig {
    std::string dispatch_service_prefix;
    std::string gateway_service_prefix;
    std::string auth_service_prefix;
    std::string room_service_prefix;
    std::string logic_service_prefix;

    int lease_ttl;
};

struct RootConfig {
    EtcdConfig etcdConfig;
    RedisConfig redisConfig;
    MysqlConfig mysqlConfig;
    KafkaConfig kafkaConfig;
    LogConfig logConfig;
    ServiceRegistryConfig serviceRegistryConfig;
};

}; // namespace pulse::config
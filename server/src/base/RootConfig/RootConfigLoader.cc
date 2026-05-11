#include "RootConfigLoader.h"

namespace pulse::config {

void RootConfigLoader::LoadRootEtcd(YAML::Node& root, EtcdConfig& config) {
    auto etcd = root["etcd"];
    if(etcd) {
        auto endpoints = etcd["endpoints"];
        if(endpoints) config.endpoints = endpoints.as<std::vector<std::string>>();
        
        auto timeout_ms = etcd["timeout_ms"];
        if(timeout_ms) {
            config.timeout_ms = timeout_ms.as<int>();
        }
    }
}

void RootConfigLoader::LoadRootRedis(YAML::Node& root, RedisConfig& config) {
    auto redis = root["redis"];
    if(redis) {
        auto endpoints = redis["endpoints"];
        if(endpoints) {
            auto endpoints_vec = endpoints.as<std::vector<YAML::Node>>();
            for(const auto& endpoint : endpoints_vec) {
                EndpointConfig endpointConfig;

                auto host = endpoint["host"];
                if(host) endpointConfig.host = host.as<std::string>();

                auto port = endpoint["port"];
                if(port) endpointConfig.port = port.as<int>();

                config.endpoints.emplace_back(endpointConfig);
            }
        }

        auto db = redis["db"];
        if(db) config.db = db.as<int>();

        auto pool_size = redis["pool_size"];
        if(pool_size) config.pool_size = pool_size.as<int>();

        auto timeout_ms = redis["timeout_ms"];
        if(timeout_ms) {
            config.timeout_ms = timeout_ms.as<int>();
        }
    }
}

void RootConfigLoader::LoadRootMysql(YAML::Node& root, MysqlConfig& config) {
    auto mysql = root["mysql"];
    if(mysql) {
        auto endpoints = mysql["endpoints"];
        if(endpoints) {
            auto endpoints_vec = endpoints.as<std::vector<YAML::Node>>();
            for(const auto& endpoint : endpoints_vec) {
                EndpointConfig endpointConfig;

                auto host = endpoint["host"];
                if(host) endpointConfig.host = host.as<std::string>();

                auto port = endpoint["port"];
                if(port) endpointConfig.port = port.as<int>();

                config.endpoints.emplace_back(endpointConfig);
            }
        }

        auto username = mysql["username"];
        if(username) config.username = username.as<std::string>();

        auto password = mysql["password"];
        if(password) config.password = password.as<std::string>();

        auto database = mysql["database"];
        if(database) config.database = database.as<std::string>();

        auto pool_thread_count = mysql["pool_thread_count"];
        if(pool_thread_count) config.pool_thread_count = pool_thread_count.as<int>();

        auto pool_size = mysql["pool_size"];
        if(pool_size) config.pool_size = pool_size.as<int>();

        auto timeout_ms = mysql["timeout_ms"];
        if(timeout_ms) {
            config.timeout_ms = timeout_ms.as<int>();
        } 
    }
}

void RootConfigLoader::LoadRootKafka(YAML::Node& root, KafkaConfig& config) {
    auto kafka = root["kafka"];
    if(kafka) {
        auto endpoints = kafka["endpoints"];
        if(endpoints) config.endpoints = endpoints.as<std::vector<std::string>>();

        auto producer = kafka["producer"];
        if(producer) {
            auto acks = producer["acks"];
            if(acks) config.producer.acks = acks.as<std::string>();

            auto enable_idempotence = producer["enable_idempotence"];
            if(enable_idempotence) 
            config.producer.enable_idempotence = enable_idempotence.as<bool>() ? "true" : "false";
        }

        auto consumer = kafka["consumer"];
        if(consumer) {
            auto group_id = consumer["group_id"];
            if(group_id) config.consumer.group_id = group_id.as<std::string>();

            auto auto_offset_reset = consumer["auto_offset_reset"];
            if(auto_offset_reset) config.consumer.auto_offset_reset = auto_offset_reset.as<std::string>();

            auto enable_auto_commit = consumer["enable_auto_commit"];
            if(enable_auto_commit) 
            config.consumer.enable_auto_commit = enable_auto_commit.as<bool>() ? "true" : "false";

            auto enable_auto_offset_store = consumer["enable_auto_offset_store"];
            if(enable_auto_offset_store) 
            config.consumer.enable_auto_offset_store = enable_auto_offset_store.as<bool>() ? "true" : "false";
        }
    }
}

void RootConfigLoader::LoadRootServiceRegistry(YAML::Node& root, ServiceRegistryConfig& config) {
    auto service_registry = root["service_registry"];
    if(service_registry) {
        auto dispatch_service_prefix = service_registry["dispatch_service_prefix"];
        if(dispatch_service_prefix) 
        config.dispatch_service_prefix = dispatch_service_prefix.as<std::string>();

        auto gateway_service_prefix = service_registry["gateway_service_prefix"];
        if(gateway_service_prefix) 
        config.gateway_service_prefix = gateway_service_prefix.as<std::string>();

        auto auth_service_prefix = service_registry["auth_service_prefix"];
        if(auth_service_prefix) 
        config.auth_service_prefix = auth_service_prefix.as<std::string>();

        auto room_service_prefix = service_registry["room_service_prefix"];
        if(room_service_prefix) 
        config.room_service_prefix = room_service_prefix.as<std::string>();

        auto logic_service_prefix = service_registry["logic_service_prefix"];
        if(logic_service_prefix) 
        config.logic_service_prefix = logic_service_prefix.as<std::string>();

        auto lease_ttl = service_registry["lease_ttl"];
        if(lease_ttl) config.lease_ttl = lease_ttl.as<int>();
    }
}

void RootConfigLoader::LoadRootLog(YAML::Node& root, LogConfig config) {
    auto log = root["log"];
    if(log) {
        auto level = log["level"];
        if(level) config.level = level.as<std::string>();
    }
}

RootConfig RootConfigLoader::LoadRoot(const std::string& path) {
    YAML::Node root = YAML::LoadFile(path);

    RootConfig config;
    LoadRootEtcd(root, config.etcdConfig);
    LoadRootRedis(root, config.redisConfig);
    LoadRootMysql(root, config.mysqlConfig);
    LoadRootKafka(root, config.kafkaConfig);
    LoadRootLog(root, config.logConfig);
    LoadRootServiceRegistry(root, config.serviceRegistryConfig);

    return config;
}
    
};
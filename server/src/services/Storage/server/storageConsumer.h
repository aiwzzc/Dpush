#pragma once

#include <memory>
#include <librdkafka/rdkafkacpp.h>

class asyncMysqlCluster;

class storageConsumer {

public:
    storageConsumer(asyncMysqlCluster* pool, const std::string& brokers, const std::string& group_id, 
        const std::vector<std::string>& topics);

    void start();

private:
    asyncMysqlCluster* mysql_pool_;
    std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
    bool running_;

};
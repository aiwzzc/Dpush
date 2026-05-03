#pragma once

#include <memory>
#include <librdkafka/rdkafkacpp.h>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <sw/redis++/redis++.h>

#include "OrderedThreadPool.h"
#include "ComputeThreadPool.h"

class grpcClient;

class KafkaConsumer {

public:
    static constexpr const char* lua_script = R"(
        local dedup_key = KEYS[1]
        local seq_key = KEYS[2]
        
        local client_msg_id = ARGV[1]
        local msg_count = tonumber(ARGV[2])

        -- 1. 幂等去重检查
        if redis.call('SADD', dedup_key, client_msg_id) == 0 then
            return {} -- 已存在
        end

        -- 2. 批量生成 msgid
        local ids = {}
        for i = 1, msg_count do
            table.insert(ids, redis.call('INCR', seq_key))
        end

        return ids
    )";

    KafkaConsumer(const std::string& brokers, const std::string& group_id, 
        const std::vector<std::string>& topics, OrderedThreadPool*, ComputeThreadPool*, sw::redis::Redis*);
    ~KafkaConsumer();

    void start_consuming();
    void start();
    void stop();

    void setgrpcClient(grpcClient*);

private:
    void process_message(RdKafka::Message* message);

    std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
    std::thread loopthread_;
    OrderedThreadPool* OrderedThreadPool_;
    ComputeThreadPool* ComputeThreadPool_;
    sw::redis::Redis* redis_pool_;
    std::atomic<bool> running_;
    std::string lua_sha1_;
    grpcClient* grpc_client_;
};
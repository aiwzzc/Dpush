#pragma once

#include <memory>
#include <librdkafka/rdkafkacpp.h>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <sw/redis++/redis++.h>

#include "../base/OrderedThreadPool.h"
#include "../base/ComputeThreadPool.h"

class KafkaConsumer {

public:
    KafkaConsumer(const std::string& brokers, const std::string& group_id, 
        std::vector<std::string> topics, OrderedThreadPool*, ComputeThreadPool*, sw::redis::Redis*);
    ~KafkaConsumer();

    void start_consuming();
    void start();
    void stop();

private:
    void process_message(RdKafka::Message* message);

    std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
    std::thread loopthread_;
    OrderedThreadPool* OrderedThreadPool_;
    ComputeThreadPool* ComputeThreadPool_;
    sw::redis::Redis* redis_pool_;
    std::atomic<bool> running_;
};
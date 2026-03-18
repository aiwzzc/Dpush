#pragma once

#include <librdkafka/rdkafkacpp.h>
#include <memory>

#include "websocketConn.h"
#include "muduo/net/TcpConnection.h"

class MyDeliveryReportCb;
class TcpConnection;

using muduo::net::TcpConnectionPtr;

struct KafkaDeliveryContext {
    std::weak_ptr<muduo::net::TcpConnection> conn_;
    std::string msg_id_;
};

class kafkaProducer {

public:
    kafkaProducer();
    ~kafkaProducer();

    RdKafka::ErrorCode produce(const std::string& topic, char* data, std::size_t data_len, 
    const std::string& key, std::size_t key_len, void* ctx, int32_t userid, std::string& username);

    void poll(int timeout = 0);

private:
    void init_kafka();

    std::unique_ptr<MyDeliveryReportCb> my_dr_cb_;
    std::unique_ptr<RdKafka::Producer> producer_;

};

using kafkaProducerPtr = std::shared_ptr<kafkaProducer>;
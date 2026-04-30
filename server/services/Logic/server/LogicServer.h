#pragma once

#include <memory>
#include <grpcpp/grpcpp.h>
#include <sw/redis++/redis++.h>

#include "OrderedThreadPool.h"
#include "LogicGrpcServer.h"
#include "KafkaConsumer.h"
#include "grpcClient.h"
#include "storage/AsyncMySQLConnPool/AsyncMysqlCluster.h"

class LogicServer {

public:
    LogicServer();
    ~LogicServer();

    void start();

private:
    static constexpr const char* brokers           = "localhost:9092";
    static constexpr const char* groupId           = "chat_room_consumer_group_1";

    std::unique_ptr<LogicGrpcServer> LogicGrpcService_;
    std::unique_ptr<Server> LogicGrpcServer_;
    std::unique_ptr<KafkaConsumer> KafkaConsumer_;
    std::unique_ptr<asyncMysqlCluster> mysql_cluster_;
    std::unique_ptr<sw::redis::Redis> redisPool_;
    std::unique_ptr<ComputeThreadPool> ComputeThreadPool_;
    std::unique_ptr<OrderedThreadPool> OrderedThreadPool_;
    std::unique_ptr<grpcClient> grpc_client_;
};
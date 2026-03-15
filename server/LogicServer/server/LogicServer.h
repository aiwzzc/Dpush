#pragma once

#include <memory>
#include <grpcpp/grpcpp.h>
#include <sw/redis++/redis++.h>

#include "../base/ComputeThreadPool.h"
#include "../base/OrderedThreadPool.h"

#include "LogicGrpcServer.h"
#include "KafkaConsumer.h"

class LogicServer {

public:
    LogicServer();
    ~LogicServer();

    void start();

private:
    static constexpr std::string dbname            = "chatroom";
    static constexpr const char* url               = "tcp://127.0.0.1:3306;root;zzc1109aiw";
    static constexpr int pool_size                 = 2;

    static constexpr std::string brokers           = "";
    static constexpr std::string groupId           = "";

    std::unique_ptr<LogicGrpcServer> LogicGrpcService_;
    std::unique_ptr<Server> LogicGrpcServer_;
    // std::unique_ptr<KafkaConsumer> KafkaConsumer_;
    std::unique_ptr<MySQLConnPool> MySQLConnPool_;
    std::unique_ptr<sw::redis::Redis> redisPool_;
    std::unique_ptr<ComputeThreadPool> ComputeThreadPool_;
    std::unique_ptr<OrderedThreadPool> OrderedThreadPool_;

};
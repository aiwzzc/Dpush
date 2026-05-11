#pragma once

#include <memory>
#include <grpcpp/grpcpp.h>
#include <sw/redis++/redis++.h>

#include "concurrency/OrderedThreadPool.h"
#include "AsyncMySQLConnPool/AsyncMysqlCluster.h"

#include "LogicGrpcServer.h"
#include "KafkaConsumer.h"
#include "grpcClient.h"
#include "LogicConfig.h"

#include "etcdServiceNode/ServiceRegistry.h"

class LogicServer {

public:
    LogicServer(pulse::config::LogicConfig& config);
    ~LogicServer();

    void start();

private:

    pulse::config::LogicConfig config_;
    pulse::net::ServiceRegistryClient serviceRegistryClient_;
    std::unique_ptr<LogicGrpcServer> LogicGrpcService_;
    std::unique_ptr<Server> LogicGrpcServer_;
    std::unique_ptr<KafkaConsumer> KafkaConsumer_;
    std::unique_ptr<asyncMysqlCluster> mysql_cluster_;
    std::unique_ptr<sw::redis::Redis> redisPool_;
    std::unique_ptr<ComputeThreadPool> ComputeThreadPool_;
    std::unique_ptr<OrderedThreadPool> OrderedThreadPool_;
    std::unique_ptr<grpcClient> grpc_client_;
};
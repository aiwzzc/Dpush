#pragma once

#include "gateway.grpc.pb.h"
#include "gateway.pb.h"

#include <memory>
#include <functional>

#include <etcd/Client.hpp>
#include <etcd/Watcher.hpp>

#include "etcdServiceNode/ServiceRegistry.h"
#include "etcdServiceNode/grpcClientPool.hpp"
#include "LogicConfig.h"

struct LogicConfig;

class grpcClient {

public:
    grpcClient(pulse::config::LogicConfig& config, 
        pulse::net::ServiceRegistryClient* serviceRegistryClient);

    void sendSingleMsgAsync(const std::string&, int32_t userid, const std::string&, 
        const std::function<void()>&);

private:
    pulse::config::LogicConfig& config_;
    pulse::net::ServiceRegistryClient* serviceRegistryClient_;
    pulse::net::RpcClientPool<gateway::GatewayServer> gateway_stubs_;

};
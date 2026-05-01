#pragma once

#include "gateway.grpc.pb.h"
#include "gateway.pb.h"

#include <etcd/Client.hpp>
#include <etcd/Watcher.hpp>

#include <mutex>
#include <string>
#include <unordered_map>
#include <memory>

class LogicDiscovery {

public:
    LogicDiscovery(const std::string& url);

    void start();

private:
    friend class grpcClient;

    using GatewayStub = std::shared_ptr<gateway::GatewayServer::Stub>;

    void etcdWatcherCallback(const etcd::Response& response);
    void addGatewayStub(const std::string& ip_port);
    void removeGatewayStub(const std::string& ip_port);
    GatewayStub getStub(const std::string& ip_port);

    std::string etcd_url_;
    std::string watch_prefix_{"/services/gateway/"};
    std::shared_ptr<etcd::Client> etcd_client_;
    std::unique_ptr<etcd::Watcher> etcd_watcher_;

    std::mutex mutex_;
    std::unordered_map<std::string, GatewayStub> gateway_stubs_;

};
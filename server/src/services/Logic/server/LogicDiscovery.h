#pragma once

#include "gateway.grpc.pb.h"
#include "gateway.pb.h"

#include <etcd/Client.hpp>
#include <etcd/Watcher.hpp>

#include "etcdServiceNode/ServiceRegistry.h"

#include <mutex>
#include <shared_mutex>
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

    void etcdWatcherCallback(const etcd::Event& event);
    void addGatewayStub(const std::string& ip_port);
    void removeGatewayStub(const std::string& ip_port);
    GatewayStub getStub(const std::string& ip_port);

    std::string etcd_url_;
    std::string watch_prefix_{"/services/gateway/"};

    pulse::net::ServiceRegistryClient ServiceRegistryClient_;

    std::shared_mutex gateway_stubs_mutex_;
    std::unordered_map<std::string, GatewayStub> gateway_stubs_;

};
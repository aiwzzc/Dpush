#include "LogicDiscovery.h"
#include <grpcpp/grpcpp.h>

LogicDiscovery::LogicDiscovery(const std::string& url):
etcd_url_(url), ServiceRegistryClient_(url) {
}

void LogicDiscovery::start() {
    this->ServiceRegistryClient_.RegisterSelf("/services/logic/", "192.168.183.130:5008");

    this->ServiceRegistryClient_.Subscribe(this->watch_prefix_, [this] (const etcd::Event& event) {
        this->etcdWatcherCallback(event);
    });

    std::vector<std::string> gateways = this->ServiceRegistryClient_.GetAllEndpoints(this->watch_prefix_).endpoints_;

    for(const auto& gateway : gateways) {
        this->addGatewayStub(gateway);
    }
}

void LogicDiscovery::etcdWatcherCallback(const etcd::Event& event) {
    std::string key = event.kv().key();

    std::string ip_port = key.substr(this->watch_prefix_.length());

    if(event.event_type() == etcd::Event::EventType::PUT) {
        this->addGatewayStub(ip_port);

    } else if(event.event_type() == etcd::Event::EventType::DELETE_) {
        this->removeGatewayStub(ip_port);
    }
}

void LogicDiscovery::addGatewayStub(const std::string& ip_port) {
    std::unique_lock<std::shared_mutex> lock(this->gateway_stubs_mutex_);
    
    auto it = this->gateway_stubs_.find(ip_port);
    if(it != this->gateway_stubs_.end()) return;

    auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
    this->gateway_stubs_[ip_port] = gateway::GatewayServer::NewStub(channel);
}

void LogicDiscovery::removeGatewayStub(const std::string& ip_port) {
    std::unique_lock<std::shared_mutex> lock(this->gateway_stubs_mutex_);
    gateway_stubs_.erase(ip_port);
}

LogicDiscovery::GatewayStub LogicDiscovery::getStub(const std::string& ip_port) {
    std::shared_lock<std::shared_mutex> lock(this->gateway_stubs_mutex_);
    auto it = this->gateway_stubs_.find(ip_port);

    return (it == this->gateway_stubs_.end()) ? nullptr : it->second;
}

#include "LogicDiscovery.h"
#include <grpcpp/grpcpp.h>

LogicDiscovery::LogicDiscovery(const std::string& url):
etcd_url_(url) {
    this->etcd_client_ = std::make_shared<etcd::Client>(this->etcd_url_);
}

void LogicDiscovery::start() {
    auto response = this->etcd_client_->ls(this->watch_prefix_).get();

    for(int i = 0; i < response.keys().size(); ++i) {
        const std::string& ip_port = response.value(i).as_string();
        this->addGatewayStub(ip_port);
    }

    this->etcd_watcher_ = std::make_unique<etcd::Watcher>(
        this->etcd_url_,
        this->watch_prefix_,
        [this] (const etcd::Response& response) {
            this->etcdWatcherCallback(response);
        },
        true
    );
}

void LogicDiscovery::etcdWatcherCallback(const etcd::Response& response) {
    for(const auto& event : response.events()) {
        std::string key = event.kv().key();

        std::string ip_port = key.substr(this->watch_prefix_.length());

        if(event.event_type() == etcd::Event::EventType::PUT) {
            this->addGatewayStub(ip_port);

        } else if(event.event_type() == etcd::Event::EventType::DELETE_) {
            this->removeGatewayStub(ip_port);
        }
    }
}

void LogicDiscovery::addGatewayStub(const std::string& ip_port) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    auto it = this->gateway_stubs_.find(ip_port);
    if(it == this->gateway_stubs_.end()) {
        auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
        this->gateway_stubs_[ip_port] = gateway::GatewayServer::NewStub(channel);
    }
}

void LogicDiscovery::removeGatewayStub(const std::string& ip_port) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    gateway_stubs_.erase(ip_port);
}

LogicDiscovery::GatewayStub LogicDiscovery::getStub(const std::string& ip_port) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    auto it = this->gateway_stubs_.find(ip_port);

    return (it == this->gateway_stubs_.end()) ? nullptr : it->second;
}

#pragma once

#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <vector>
#include <memory>

#include <grpcpp/grpcpp.h>

#include "ServiceRegistry.h"

namespace pulse::net {

template<typename T>
class RpcClientPool {

public:
    void Init(ServiceRegistryClient& registry, const std::string& prefix) {
        registry.Subscribe(prefix, [this, &prefix] (const etcd::Event& event) {
            std::string key = event.kv().key();
            std::string endpoint = key.substr(prefix.length());

            if(event.event_type() == etcd::Event::EventType::PUT) {
                AddStub(endpoint);

            } else if(event.event_type() == etcd::Event::EventType::DELETE_) {
                RemoveStub(endpoint);
            }
        });

        std::vector<std::string> endpoints = registry.GetAllEndpoints(prefix).endpoints_;
        for(const std::string& endpoint : endpoints) {
            this->AddStub(endpoint);
        }
    }

    std::shared_ptr<typename T::Stub> GetStub(ServiceRegistryClient& registry, const std::string& prefix) {
        std::string endpoint = registry.GetEndpoint(prefix);

        std::shared_lock<std::shared_mutex> lock(this->mutex_);

        auto it = this->stubs_.find(endpoint);
        return it != this->stubs_.end() ? it->second : nullptr;
    }

private:

    void AddStub(const std::string& endpoint) {
        std::unique_lock<std::shared_mutex> lock(this->mutex_);

        if(this->stubs_.contains(endpoint)) return;

        auto channel = grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
        this->stubs_[endpoint] = T::NewStub(channel);
    }

    void RemoveStub(const std::string& endpoint) {
        std::unique_lock<std::shared_mutex> lock(this->mutex_);
        this->stubs_.erase(endpoint);
    }

private:
    std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<typename T::Stub>> stubs_;

};

}; // namespace pulse::net

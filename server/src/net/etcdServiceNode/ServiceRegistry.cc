#include "ServiceRegistry.h"
#include <algorithm>

namespace pulse::net {

ServiceRegistryClient::ServiceRegistryClient(const std::string& etcd_endpoints):
    etcd_endpoints_(etcd_endpoints), etcd_client_(etcd_endpoints) {}

void ServiceRegistryClient::RegisterSelf(const std::string& service_prefix, const std::string& service_url, int ttl) {
    auto lease_res = this->etcd_client_.leasegrant(ttl).get();
    this->lease_id = lease_res.value().lease();

    this->my_registered_key_ = service_prefix + service_url;

    this->etcd_client_.put(this->my_registered_key_, service_url, this->lease_id).get();
    this->etcd_keep_alive_ = std::make_shared<etcd::KeepAlive>(this->etcd_client_, ttl, this->lease_id);
}

void ServiceRegistryClient::Subscribe(const std::string& target_prefix, const WatcherCallback& cb) {
    if(auto it = this->watchers_.find(target_prefix);
        it == this->watchers_.end()) {

        this->FetchInitialEndpoints(target_prefix);

        auto watcher = std::make_unique<etcd::Watcher>(
            this->etcd_endpoints_,
            target_prefix,
            [this, target_prefix, cb = std::move(cb)] (const etcd::Response& response) {
                for(const auto& event : response.events()) {
                    this->OnWatchEvent(target_prefix, event);
                    if(cb) cb(event);
                }
            },
            true
        );

        this->watchers_[target_prefix] = std::move(watcher);
    }
}

NodePool ServiceRegistryClient::GetAllEndpoints(const std::string& target_prefix) {
    std::shared_lock<std::shared_mutex> lock(this->cache_mutex_);
    auto it = this->endpoints_cache_.find(target_prefix);
    if(it == this->endpoints_cache_.end()) return {};

    return it->second;
}

std::string ServiceRegistryClient::GetEndpoint(const std::string& target_prefix) {
    std::shared_lock<std::shared_mutex> lock(this->cache_mutex_);
    auto it = this->endpoints_cache_.find(target_prefix);
    if(it == this->endpoints_cache_.end()) return {};

    std::size_t& index = this->round_robin_idx_[target_prefix];
    std::string endpoint = it->second.endpoints_[index % it->second.endpoints_.size()];
    ++index;

    return endpoint;
}

void ServiceRegistryClient::OnWatchEvent(const std::string& target_prefix, const etcd::Event& event) {
    std::string key = event.kv().key();
    std::string endpoints = key.substr(target_prefix.length());

    if(event.event_type() == etcd::Event::EventType::PUT) {
        std::unique_lock<std::shared_mutex> lock(this->cache_mutex_);

        auto& pool = this->endpoints_cache_[target_prefix];

        if(pool.index_map_.contains(endpoints)) return;

        std::size_t index = pool.endpoints_.size();
        pool.index_map_[endpoints] = index;
        pool.endpoints_.emplace_back(endpoints);

    } else if(event.event_type() == etcd::Event::EventType::DELETE_) {
        std::unique_lock<std::shared_mutex> lock(this->cache_mutex_);

        auto it = this->endpoints_cache_.find(target_prefix);
        if(it == this->endpoints_cache_.end()) return;

        auto& node = it->second;

        auto map_it = node.index_map_.find(endpoints);
        if(map_it == node.index_map_.end()) return;

        std::size_t remove_index = map_it->second;
        std::size_t last_index = node.endpoints_.size() - 1;

        if(remove_index != last_index) {
            std::swap(node.endpoints_[remove_index], node.endpoints_[last_index]);

            node.index_map_[node.endpoints_[remove_index]] = remove_index;
        }

        node.endpoints_.pop_back();
        node.index_map_.erase(map_it);
    }
}

void ServiceRegistryClient::FetchInitialEndpoints(const std::string& target_prefix) {
    auto response = this->etcd_client_.ls(target_prefix).get();

    std::unique_lock<std::shared_mutex> lock(this->cache_mutex_);

    this->endpoints_cache_[target_prefix].endpoints_.clear();
    this->endpoints_cache_[target_prefix].index_map_.clear();
    
    for(int i = 0; i < response.keys().size(); ++i) {
        std::string service_endpoint = response.value(i).as_string();
        std::size_t index = this->endpoints_cache_[target_prefix].endpoints_.size();
        this->endpoints_cache_[target_prefix].index_map_[service_endpoint] = index;
        this->endpoints_cache_[target_prefix].endpoints_.emplace_back(service_endpoint);
    }
}

}; // namespace pulse::net
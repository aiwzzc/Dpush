#pragma once

#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Watcher.hpp>

#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>
#include <shared_mutex>
#include <memory>
#include <functional>
#include <unordered_set>
#include <atomic>

namespace pulse::net {

// swap + pop_back 
struct NodePool {
    std::vector<std::string> endpoints_;
    std::unordered_map<std::string, std::size_t> index_map_;
    std::atomic<std::size_t> index_{0};
};

struct NodePoolRes {
    std::vector<std::string> endpoints_;
    std::unordered_map<std::string, std::size_t> index_map_;
};

class ServiceRegistryClient {

public:
    using WatcherCallback = std::function<void(const etcd::Event&)>;

    explicit ServiceRegistryClient(const std::string& etcd_endpoints);

    void RegisterSelf(const std::string& service_prefix, const std::string& service_url, int ttl = 30);
    void Subscribe(const std::string& target_prefix, const WatcherCallback& cb);
    NodePoolRes GetAllEndpoints(const std::string& target_prefix);
    std::string GetEndpoint(const std::string& target_prefix);

private:
    void OnWatchEvent(const std::string& target_prefix, const etcd::Event& event);
    void FetchInitialEndpoints(const std::string& target_prefix);

private:
    std::string etcd_endpoints_;
    etcd::Client etcd_client_;
    
    int64_t lease_id{0};
    std::string my_registered_key_;
    std::shared_ptr<etcd::KeepAlive> etcd_keep_alive_;

    std::shared_mutex cache_mutex_;
    std::unordered_map<std::string, NodePool> endpoints_cache_;

    std::unordered_map<std::string, std::unique_ptr<etcd::Watcher>> watchers_;
};

}; // namespace pulse::net


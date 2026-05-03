#pragma once

#include "cpp-httplib/httplib.h"
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_set>

#include <etcd/Client.hpp>
#include <etcd/Watcher.hpp>

#include <sw/redis++/redis++.h>

struct GatewayInfo {
    int score;
    std::string gateway_url;
};

class dispatchServer {

public:
    dispatchServer(const std::string& etcd_url);
    ~dispatchServer();

    void start();

private:
    void BackendSyncTask();
    void etcdWatcherCallback(const etcd::Response& response);
    void httpEventCallback(const httplib::Request& req, httplib::Response& res);

    std::string etcd_url_;
    std::string watch_prefix_{"/services/gateway/"};

    std::shared_ptr<etcd::Client> etcd_client_;
    std::unique_ptr<etcd::Watcher> etcd_watcher_;
    std::unique_ptr<httplib::Server> server_;

    std::unordered_set<std::string> etcd_conns_;
    std::shared_mutex etcd_conns_mutex_;
    std::vector<GatewayInfo> cached_gateways_;
    std::shared_mutex cache_mutex_;

    bool running_{true};
    std::thread sync_worker_;
    std::unique_ptr<sw::redis::Redis> redisPool_;
};
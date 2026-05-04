#pragma once

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

#include "httpServer/HttpServer.h"
#include "auth.grpc.pb.h"
#include "auth.pb.h"
#include "coroutineTask.h"

class HttpRequest;
class HttpResponse;

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
    void onetcdWatcher(const etcd::Response& response);
    DetachedTask onDispatch(const TcpConnectionPtr& conn, const HttpRequest& req);
    DetachedTask onLogin(const TcpConnectionPtr& conn, const HttpRequest& req);
    DetachedTask onRegister(const TcpConnectionPtr& conn, const HttpRequest& req);
    DetachedTask onjoinSession(const TcpConnectionPtr& conn, const HttpRequest& req);
    DetachedTask oncreateSession(const TcpConnectionPtr& conn, const HttpRequest& req);

    std::string etcd_url_;
    std::string watch_prefix_{"/services/gateway/"};

    std::shared_ptr<etcd::Client> etcd_client_;
    std::unique_ptr<etcd::Watcher> etcd_watcher_;
    std::unique_ptr<HttpServer> server_;

    std::shared_ptr<grpc::Channel> Authchannel;
    std::unique_ptr<auth::AuthServer::Stub> Authstub;

    std::unordered_set<std::string> etcd_conns_;
    std::shared_mutex etcd_conns_mutex_;
    std::vector<GatewayInfo> cached_gateways_;
    std::shared_mutex cache_mutex_;

    bool running_{true};
    std::thread sync_worker_;
    std::unique_ptr<sw::redis::Redis> redisPool_;
};
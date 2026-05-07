#pragma once

#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_set>
#include <optional>

#include <etcd/Client.hpp>
#include <etcd/Watcher.hpp>
#include <sw/redis++/redis++.h>

#include "http/HttpServer.h"
#include "auth.grpc.pb.h"
#include "auth.pb.h"
#include "logic.grpc.pb.h"
#include "logic.pb.h"

#include "concurrency/coroutineTask.h"

class HttpRequest;
class HttpResponse;

struct GatewayInfo {
    int score;
    std::string gateway_url;
};

struct LoginInfo {
    int errcode{-1};
    std::string errmsg;
    std::string token;
    int32_t userid{0};
    std::string username;
};

struct RegisterInfo {
    int errcode{-1};
    std::string errmsg;
};

struct CreateSessionInfo {
    int errcode{-1};
    std::string errmsg;
    int32_t userid{0};
    int64_t room_id;
};

struct JoinSessionInfo {
    int errcode{-1};
    std::string errmsg;
    int32_t userid{0};
    int64_t room_id;
};

class dispatchServer {

public:
    enum class api_error_id {
        bad_request = -6,
        login_failed_password_error,
        login_failed_email_error,
        email_exists,
        username_exists,
        Unknown_error
    };

    // static std::optional<api_error_id> to_api_error_id(int v);
    // static std::string api_error_id_to_string(api_error_id id);

    dispatchServer(const std::string& etcd_url);
    ~dispatchServer();

    void start();

private:
    void BackendSyncTask();
    void onetcdWatcher(const etcd::Response& response);
    DetachedTask onDispatch(TcpConnectionPtr conn, HttpRequest req);
    DetachedTask onLogin(TcpConnectionPtr conn, HttpRequest req);
    DetachedTask onRegister(TcpConnectionPtr conn, HttpRequest req);
    DetachedTask onjoinSession(TcpConnectionPtr conn, HttpRequest req);
    DetachedTask oncreateSession(TcpConnectionPtr conn, HttpRequest req);

    std::string etcd_url_;
    std::string watch_prefix_{"/services/gateway/"};

    std::shared_ptr<etcd::Client> etcd_client_;
    std::unique_ptr<etcd::Watcher> etcd_watcher_;
    std::unique_ptr<HttpServer> server_;

    std::shared_ptr<grpc::Channel> Authchannel;
    std::unique_ptr<auth::AuthServer::Stub> Authstub;

    std::shared_ptr<grpc::Channel> Logicchannel;
    std::unique_ptr<logic::LogicServer::Stub> Logicstub;

    std::unordered_set<std::string> etcd_conns_;
    std::shared_mutex etcd_conns_mutex_;
    std::vector<GatewayInfo> cached_gateways_;
    std::shared_mutex cache_mutex_;

    bool running_{true};
    std::thread sync_worker_;
    std::unique_ptr<sw::redis::Redis> redisPool_;
};
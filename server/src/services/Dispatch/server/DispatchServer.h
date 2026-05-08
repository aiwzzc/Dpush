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

#include <grpcpp/grpcpp.h>

#include "concurrency/coroutineTask.h"
#include "etcdServiceNode/ServiceRegistry.h"

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

    dispatchServer(const std::string& etcd_url);
    ~dispatchServer();

    void start();

private:
    void BackendSyncTask();
    DetachedTask onDispatch(TcpConnectionPtr conn, HttpRequest req);
    DetachedTask onLogin(TcpConnectionPtr conn, HttpRequest req);
    DetachedTask onRegister(TcpConnectionPtr conn, HttpRequest req);
    DetachedTask onjoinSession(TcpConnectionPtr conn, HttpRequest req);
    DetachedTask oncreateSession(TcpConnectionPtr conn, HttpRequest req);

private:

    template<typename StubMap, typename StubFactory>
    void addStub(StubMap& stubmap, std::shared_mutex& mutex, 
        const std::string& endpoint, StubFactory&& factory) {

        std::unique_lock<std::shared_mutex> lock(mutex);

        if(stubmap.contains(endpoint)) return;

        auto channel = grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
        stubmap[endpoint] = factory(channel);
    }

    template<typename StubMap>
    void removeStub(StubMap& stubmap, std::shared_mutex& mutex, const std::string& endpoint) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        stubmap.erase(endpoint);
    }

    template<typename StubMap, typename StubFactory>
    void onWatcher(const etcd::Event& event, const std::string& service_prefix, 
        StubMap& stubmap, std::shared_mutex& mutex, StubFactory&& factory) {

        std::string key = event.kv().key();
        std::string endpoint = key.substr(service_prefix.length());

        auto type = event.event_type();

        if(type == etcd::Event::EventType::DELETE_) {
            removeStub(stubmap, mutex, endpoint);

            return;
        }

        if(type == etcd::Event::EventType::PUT) {
            addStub(stubmap, mutex, endpoint, factory);
        }
    }

private:
    using AuthStub = std::unique_ptr<auth::AuthServer::Stub>;
    using LogicStub = std::unique_ptr<logic::LogicServer::Stub>;

    std::string etcd_url_;
    std::string gateway_prefix_{"/services/gateway/"};
    std::string auth_prefix_{"/services/auth/"};
    std::string logic_prefix_{"/services/logic/"};

    // std::shared_ptr<etcd::Client> etcd_client_;
    // std::unique_ptr<etcd::Watcher> etcd_watcher_;
    std::unique_ptr<HttpServer> server_;

    // std::shared_ptr<grpc::Channel> Authchannel;
    // std::unique_ptr<auth::AuthServer::Stub> Authstub;

    // std::shared_ptr<grpc::Channel> Logicchannel;
    // std::unique_ptr<logic::LogicServer::Stub> Logicstub;

    // std::unordered_set<std::string> etcd_conns_;
    // std::shared_mutex etcd_conns_mutex_;

    std::shared_mutex auth_stubs_mutex_;
    std::unordered_map<std::string, AuthStub> auth_stubs_;

    std::shared_mutex logic_stubs_mutex_;
    std::unordered_map<std::string, LogicStub> logic_stubs_;

    pulse::net::ServiceRegistryClient ServiceRegistryClient_;

    std::vector<GatewayInfo> cached_gateways_;
    std::shared_mutex cache_mutex_;

    bool running_{true};
    std::thread sync_worker_;
    std::unique_ptr<sw::redis::Redis> redisPool_;
};
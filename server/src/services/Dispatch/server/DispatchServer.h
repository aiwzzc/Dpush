#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <shared_mutex>
#include <thread>
#include <optional>

#include <etcd/Client.hpp>
#include <etcd/Watcher.hpp>
#include <sw/redis++/redis++.h>

#include "http/HttpServer.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

#include "constants/http_constants.h"

#include "auth.grpc.pb.h"
#include "logic.grpc.pb.h"

#include <grpcpp/grpcpp.h>

#include "GrpcAwaiter.hpp"
#include "concurrency/coroutineTask.h"
#include "etcdServiceNode/grpcClientPool.hpp"

#include "DispatchConfig.h"

struct GatewayInfo {
    int score;
    std::string gateway_url;
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

    dispatchServer(pulse::config::DispatchConfig& config);
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
    template<typename Info, typename Builder>
    void sendHttpResponse(
        const TcpConnectionPtr& conn, 
        const Info& info, 
        HttpRequest& req, 
        Builder&& builder) {

        auto connection_opt = req.getHeader("Connection");

        const std::string& connection = connection_opt.has_value() ? *(connection_opt.value()) : "";
        bool close = connection == "close" ||
            (req.version() == HttpRequest::Version::kHttp10 && connection != "Keep-Alive");

        HttpResponse res(close);

        if(info.errcode < 0) {
            res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
            res.setStatusMessage(pulse::constants::http::MSG_BAD_REQUEST);

        } else {
            res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
            res.setStatusMessage(pulse::constants::http::MSG_OK);
        }

        res.setCloseConnection(true);
        res.setContentType(std::string(pulse::constants::http::CONTENT_TYPE_OCTET));

        res.setBody(builder(info));

        std::string output;
        res.appendToBuffer(output);

        conn->send(output);
        if (res.closeConnection()) {
            conn->shutdown();
        }
    }

    template<typename GrpcReq, typename GrpcRes, typename StubPool,
    typename HttpReqParser, typename GprcInvoker, typename HttpResHandler>
    DetachedTask GenericApiHandler(
        TcpConnectionPtr conn, 
        HttpRequest req, StubPool& stubPool, 
        pulse::net::ServiceRegistryClient& ServiceRegistryClient, 
        const std::string& prefix,
        HttpReqParser req_parser, 
        GprcInvoker grpc_invoker, 
        HttpResHandler res_handler) {

        GrpcReq grpc_req = req_parser(req.body().data());

        auto stub = stubPool.GetStubFromPrefix(ServiceRegistryClient, prefix);
        if(stub == nullptr) co_return;

        auto [status, grpc_res] = co_await MakeGrpcAwaiter<GrpcRes>(
            [&grpc_invoker, stub, &grpc_req] (grpc::ClientContext* context, GrpcRes* response, auto cb) {
                grpc_invoker(stub, context, &grpc_req, response, std::move(cb));
            }
        );

        if(!status.ok()) co_return;

        res_handler(conn, req, grpc_res.get());
    }

private:

    using HttpHandler = std::function<DetachedTask(const TcpConnectionPtr&, const HttpRequest&)>;
    std::vector<std::pair<std::string_view, HttpHandler>> HttpApiRoutes_;

private:
    using AuthStub = std::unique_ptr<auth::AuthServer::Stub>;
    using LogicStub = std::unique_ptr<logic::LogicServer::Stub>;

    pulse::config::DispatchConfig config_;

    std::unique_ptr<HttpServer> server_;

    pulse::net::ServiceRegistryClient ServiceRegistryClient_;

    pulse::net::RpcClientPool<auth::AuthServer> auth_client_pool_;
    pulse::net::RpcClientPool<logic::LogicServer> logic_client_pool_;

    std::vector<GatewayInfo> cached_gateways_;
    std::shared_mutex cache_mutex_;

    bool running_{true};
    std::thread sync_worker_;
    std::unique_ptr<sw::redis::Redis> redisPool_;
};
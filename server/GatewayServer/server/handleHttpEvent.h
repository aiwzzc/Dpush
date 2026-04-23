#pragma once

#include "muduo/net/TcpConnection.h"
#include "grpcClient.h"

#include <unordered_map>
#include <string>
#include <string_view>

class HttpRequest;

using muduo::net::TcpConnectionPtr;

struct HttpEventContext {
    TcpConnectionPtr conn_;
    grpcClientPtr rpc_;
    const HttpRequest& req_;
    bool close_;
    HttpResponse& res_;
};

struct HttpEventDef {
    std::string name_;
    std::function<void(HttpEventContext&)> execute_;
};

class HttpEventRegister {

private:
    std::unordered_map<std::string, HttpEventDef> event_table_;
    void registerEvent(HttpEventDef def);

public:
    HttpEventRegister();

    HttpEventDef* loop_up(const std::string& name);
    static HttpEventRegister& getInstance();
};

class HttpEventHandlers {

public:
    static HttpEventHandlers& getInstance();

    void handleHttpEvent(const TcpConnectionPtr&, const HttpRequest&, const grpcClientPtr&);

    static void register_api(HttpEventContext& ctx);
    static void login_api(HttpEventContext& ctx);
    static void joinSession_api(HttpEventContext& ctx);
    static void createSession_api(HttpEventContext& ctx);

};
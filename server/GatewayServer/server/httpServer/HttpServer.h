#pragma once

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"

#include <string>
#include <iostream>
#include <memory>
#include <cstring>
#include <functional>
#include <unordered_map>

#define HTTP_RESPONSE_JSON_MAX 4096
#define HTTP_RESPONSE_JSON                                                     \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"

#define HTTP_RESPONSE_WITH_CODE                                                     \
    "HTTP/1.1 %d %s\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"

// 86400单位是秒，86400换算后是24小时
#define HTTP_RESPONSE_WITH_COOKIE                                                    \
    "HTTP/1.1 %d %s\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "set-cookie: sid=%s; HttpOnly; Max-Age=86400; SameSite=Strict\r\n" \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"


#define HTTP_RESPONSE_HTML_MAX 4096
#define HTTP_RESPONSE_HTML                                                    \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:text/html;charset=utf-8\r\n\r\n%s"


#define HTTP_RESPONSE_BAD_REQ                                                     \
    "HTTP/1.1 400 Bad\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"

class TcpConnection;

using muduo::net::TcpConnectionPtr;
using muduo::net::EventLoop;
using muduo::net::TcpServer;

class HttpRequest;
class HttpResponse;

class HttpServer {

public:
    using HttpCallback = std::function<void(TcpConnectionPtr conn, HttpRequest)>;
    using UpgradeCallback = std::function<void(const TcpConnectionPtr&, const HttpRequest&)>;

    // HttpServer(const muduo::net::InetAddress &addr, const std::string& name, int num_event_loops);
    HttpServer(uint16_t start_port, int port_count, const std::string& name, int total_event_loops);
    ~HttpServer();

    static std::unordered_map<std::string, std::string> StaticFilesHash;

    void start();
    void setHttpCallback(const HttpCallback& cb);
    void setUpgradeCallback(const UpgradeCallback& cb);

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, std::string buf);
    void onRequest(const TcpConnectionPtr& conn, const HttpRequest& request);
    void defaultHttpCallback(const TcpConnectionPtr&, const HttpRequest&);

    std::unique_ptr<EventLoop> loop_;
    // std::unique_ptr<TcpServer> server_;
    std::vector<std::unique_ptr<TcpServer>> servers_;
    HttpCallback httpCallback_;
    UpgradeCallback upgradeCallback_;
};
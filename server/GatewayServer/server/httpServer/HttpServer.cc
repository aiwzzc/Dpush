#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpContext.h"
#include "HttpResponse.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpConnection.h"

#include "../GatewayPubSubManager.h"

HttpServer::HttpServer(const muduo::net::InetAddress &addr, const std::string& name, int num_event_loops):
    loop_(std::make_unique<EventLoop>()), 
    server_(std::make_unique<TcpServer>(this->loop_.get(), addr, name)) {
    this->server_->setMessageCallback([this] (const TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp) {
        onMessage(conn, buf->retrieveAsString(buf->readableBytes()));
    });

    this->server_->setConnectionCallback([this] (const TcpConnectionPtr& conn) { onConnection(conn); });
    setHttpCallback([this] (const TcpConnectionPtr& conn, const HttpRequest& req) { defaultHttpCallback(conn, req); });

    this->server_->setThreadNum(num_event_loops);

    this->server_->setThreadInitCallback([] (EventLoop* loop) {
        GatewayPubSubManager::RegisterLoop(loop);
    });
}

HttpServer::~HttpServer() = default;

void HttpServer::start() {
    this->server_->start();
    this->loop_->loop(1000);
}

void HttpServer::setHttpCallback(const HttpCallback& cb) { this->httpCallback_ = std::move(cb); }

void HttpServer::setUpgradeCallback(const UpgradeCallback& cb) { this->upgradeCallback_ = std::move(cb); }

void HttpServer::onConnection(const TcpConnectionPtr& conn) {
    conn->setContext(HttpContext{});
}

void HttpServer::onMessage(const TcpConnectionPtr& conn, std::string buf) {
    if(conn->disconnected()) return;

    HttpContext* context = std::any_cast<HttpContext>(conn->getMutableContext());
    if(context == nullptr) return;

    if(!context->parseRequest(buf)) {
        std::string badStr("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->send(badStr);
        conn->shutdown();
        return;
    }

    if(context->gotAll()) {
        if(context->request().getHeader("Connection").find("Upgrade") != std::string::npos) {
            if(this->upgradeCallback_) {
                this->upgradeCallback_(conn, context->request());

            } else {
                // 返回 501 Not Implemented 拒绝
            }

        } else {
            onRequest(conn, context->request());
            if(context->request().getHeader("Connection") == "keep-alive") context->reset();
        }
    }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& request) {
    if(conn->disconnected()) return;

    this->httpCallback_(conn, request);
}

void HttpServer::defaultHttpCallback(const muduo::net::TcpConnectionPtr& conn, const HttpRequest& req) {
    if(conn->disconnected()) return;

    HttpResponse res{true};
    res.setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
    res.setStatusMessage("Not Found");
    res.setCloseConnection(true);
    
    std::string res_json{};
    res.appendToBuffer(res_json);
    conn->send(res_json);
}
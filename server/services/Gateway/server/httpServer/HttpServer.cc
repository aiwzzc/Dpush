#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpContext.h"
#include "HttpResponse.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/base/Logging.h"

#include "../GatewayPubSubManager.h"

#include <fstream>
#include <sstream>

std::unordered_map<std::string, std::string> HttpServer::StaticFilesHash{};

static std::string readFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// HttpServer::HttpServer(const muduo::net::InetAddress &addr, const std::string& name, int num_event_loops):
//     loop_(std::make_unique<EventLoop>()), 
//     server_(std::make_unique<TcpServer>(this->loop_.get(), addr, name, TcpServer::kReusePort)) {
//     this->server_->setMessageCallback([this] (const TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp) {
//         onMessage(conn, buf->retrieveAsString(buf->readableBytes()));
//     });

//     this->server_->setConnectionCallback([this] (const TcpConnectionPtr& conn) { onConnection(conn); });
//     setHttpCallback([this] (const TcpConnectionPtr& conn, const HttpRequest& req) { defaultHttpCallback(conn, req); });

//     this->server_->setThreadNum(num_event_loops);

//     this->server_->setThreadInitCallback([] (EventLoop* loop) {
//         GatewayPubSubManager::RegisterLoop(loop);
//     });

// }

HttpServer::HttpServer(uint16_t start_port, int port_count, const std::string& name, int total_event_loops):
    loop_(std::make_unique<EventLoop>()) {

    int threads_per_server = total_event_loops / port_count;
    if (threads_per_server < 1) threads_per_server = 1;

    for(int i = 0; i < port_count; ++i) {
        uint16_t port = start_port + i;
        muduo::net::InetAddress addr{"0.0.0.0", port};

        std::string server_name = name + "-" + std::to_string(port);

        auto server = std::make_unique<TcpServer>(this->loop_.get(), addr, server_name, TcpServer::kReusePort);

        server->setMessageCallback([this] (const TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp) {
            onMessage(conn, buf);
        });

        server->setConnectionCallback([this] (const TcpConnectionPtr& conn) { onConnection(conn); });

        server->setThreadNum(threads_per_server);

        this->servers_.emplace_back(std::move(server));
    }

    setHttpCallback([this] (const TcpConnectionPtr& conn, const HttpRequest& req) { 
        defaultHttpCallback(conn, req); 
    });
}

HttpServer::~HttpServer() = default;

// void HttpServer::start() {
//     this->server_->start();
//     this->loop_->loop(1000);
// }

void HttpServer::start() {
    for (auto& server : servers_) {
        server->start();
    }

    this->loop_->loop(1000);
}

void HttpServer::setHttpCallback(const HttpCallback& cb)
{ this->httpCallback_ = std::move(cb); }

void HttpServer::setUpgradeCallback(const UpgradeCallback& cb)
{ this->upgradeCallback_ = std::move(cb); }

void HttpServer::setThreadInitCallback(const ThreadInitCallback& cb) {
    for(auto& server : servers_) {
        server->setThreadInitCallback(cb);
    }
}

void HttpServer::onConnection(const TcpConnectionPtr& conn) {
    conn->setTcpNoDelay(true);
    conn->setContext(HttpContext{});

    conn->setHighWaterMarkCallback([](const TcpConnectionPtr& c, size_t len) {
            LOG_WARN << "HighWaterMark triggered! OutputBuffer size: " << len 
                        << " bytes. Client receiving too slow or Server sending too fast!";
        }, 
        64 * 1024
    );
}

void HttpServer::onMessage(const TcpConnectionPtr& conn, muduo::net::Buffer* buf) {
    if(conn->disconnected()) return;
// std::cout << std::string_view(buf->peek(), buf->readableBytes()) << std::endl;
    HttpContext* context = std::any_cast<HttpContext>(conn->getMutableContext());
    if(context == nullptr) return;

    if(!context->parseRequest(buf)) {
        std::string_view badStr("HTTP/1.1 400 Bad Request\r\n\r\n");
        std::cout << badStr << std::endl;
        conn->send(badStr.data(), badStr.size());
        conn->shutdown();
        return;
    }

    if(context->gotAll()) {
        auto connection_opt = context->request().getHeader("Connection");

        if(connection_opt.has_value() && (*connection_opt.value()).find("Upgrade") != std::string::npos) {
            if(this->upgradeCallback_) {
                this->upgradeCallback_(conn, context->request());

            } else {
                // 返回 501 Not Implemented 拒绝
            }

        } else {
            onRequest(conn, context->request());
            if(connection_opt.has_value() && *connection_opt.value() == "keep-alive") context->reset();
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
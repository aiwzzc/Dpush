#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpContext.h"
#include "HttpResponse.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/base/Logging.h"

HttpServer::HttpServer(const muduo::net::InetAddress &addr, const std::string& name, int num_event_loops):
    loop_(std::make_unique<EventLoop>()), 
    server_(std::make_unique<TcpServer>(this->loop_.get(), addr, name, TcpServer::kReusePort)) {

    this->server_->setMessageCallback([this] (const TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp) {
        onMessage(conn, buf);
    });

    // this->server_->setConnectionCallback([this] (const TcpConnectionPtr& conn) { onConnection(conn); });
    // setHttpCallback([this] (const HttpRequest& req, HttpResponse& res) { defaultHttpCallback(req, res); });

    this->server_->setThreadNum(num_event_loops);
}

HttpServer::~HttpServer() = default;

void HttpServer::start() {
    this->server_->start();

    this->loop_->loop(1000);
}

// void HttpServer::setHttpCallback(const HttpCallback& cb)
// { this->httpCallback_ = std::move(cb); }

void HttpServer::setThreadInitCallback(const ThreadInitCallback& cb) {
    this->server_->setThreadInitCallback(cb);
}

void HttpServer::Get(const std::string& path, const HttpCallback& cb) {
    auto it = this->routes.find(path);
    if(it == this->routes.end()) {
        this->routes[path] = std::move(cb);
    }
}

void HttpServer::GetAsync(const std::string& path, const AsyncHttpCallback& cb) {
    auto it = this->coro_routes.find(path);
    if(it == this->coro_routes.end()) {
        this->coro_routes[path] = std::move(cb);
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
        
        onRequest(conn, context->request());
        context->reset();
    }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& request) {
    if(conn->disconnected()) return;

    const std::string& key = request.path();
    auto it = this->coro_routes.find(key);
    if(it == this->coro_routes.end()) {
        defaultHttpCallback(conn, request);
        
    } else {
        it->second(conn, request);
    }
}

void HttpServer::defaultHttpCallback(const TcpConnectionPtr& conn, const HttpRequest& request) {
    if(conn->disconnected()) return;

    HttpResponse res{true};
    res.setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
    res.setStatusMessage("Not Found");
    res.setCloseConnection(true);

    std::string output;
    res.appendToBuffer(output);
    conn->send(output);
    if (res.closeConnection()) {
        conn->shutdown();
    }
}
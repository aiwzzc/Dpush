#include "DispatchServer.h"
#include "yyjson/JsonView.h"

#include "httpServer/HttpRequest.h"
#include "httpServer/HttpResponse.h"

#include "muduo/net/InetAddress.h"
#include <grpcpp/grpcpp.h>
#include <coroutine>

dispatchServer::dispatchServer(const std::string& etcd_url) :
etcd_url_(etcd_url) {
    this->etcd_client_ = std::make_shared<etcd::Client>(this->etcd_url_);
    this->server_ = std::make_unique<HttpServer>(muduo::net::InetAddress{"0.0.0.0", 5001}, "HttpServer", 6);

    this->Authchannel = grpc::CreateChannel("127.0.0.1:5006", grpc::InsecureChannelCredentials());
    this->Authstub = auth::AuthServer::NewStub(this->Authchannel);

    sw::redis::ConnectionOptions connection_options;
    connection_options.host = "127.0.0.1";
    connection_options.port = 6379;
    connection_options.db = 1;

    sw::redis::ConnectionPoolOptions pool_options;
    pool_options.size = 3;

    this->redisPool_ = std::make_unique<sw::redis::Redis>(connection_options, pool_options);
}

dispatchServer::~dispatchServer() {
    this->running_ = false;
    if(this->sync_worker_.joinable()) this->sync_worker_.join();
}

void dispatchServer::BackendSyncTask() {
    while(this->running_) {
        std::unordered_map<std::string, std::string> gateway_loads;
        this->redisPool_->hgetall("gateway:load", std::inserter(gateway_loads, gateway_loads.begin()));

        std::vector<GatewayInfo> temp_list;
        JsonDoc root;

        for(auto it = gateway_loads.begin(); it != gateway_loads.end(); ++it) {
            std::string load_json = it->second;

            if(root.parse(load_json.data(), load_json.size())) {
                int score = root.root()["conn"].asInt();
                temp_list.emplace_back(score, it->first);
            }
        }

        std::sort(temp_list.begin(), temp_list.end(), 
        [] (const GatewayInfo& a, const GatewayInfo& b) {
            return a.score < b.score;
        });

        {
            std::unique_lock<std::shared_mutex> lock(this->cache_mutex_);
            this->cached_gateways_ = std::move(temp_list);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

void dispatchServer::onetcdWatcher(const etcd::Response& response) {
    for(const auto& event : response.events()) {
        std::string key = event.kv().key();

        std::string ip_port = key.substr(this->watch_prefix_.length());

        if(event.event_type() == etcd::Event::EventType::PUT) {
            {
                std::unique_lock<std::shared_mutex> lock(this->etcd_conns_mutex_);
                this->etcd_conns_.insert(ip_port);
            }

        } else if(event.event_type() == etcd::Event::EventType::DELETE_) {
            {
                std::unique_lock<std::shared_mutex> lock(this->etcd_conns_mutex_);
                this->etcd_conns_.erase(ip_port);
            }
        }
    }
}

static bool req_is_close(const HttpRequest& req) {
    auto connection_opt = req.getHeader("Connection");

    const std::string& connection = connection_opt.has_value() ? *(connection_opt.value()) : "";
    bool close = connection == "close" ||
        (req.version() == HttpRequest::Version::kHttp10 && connection != "Keep-Alive");

    return close;
}

static void sendRes(const TcpConnectionPtr& conn, HttpResponse& res) {
    std::string output;
    res.appendToBuffer(output);
    conn->send(output);
    if (res.closeConnection()) {
        conn->shutdown();
    }
}

DetachedTask dispatchServer::onDispatch(const TcpConnectionPtr& conn, const HttpRequest& req) {
    bool close = req_is_close(req);
    HttpResponse res(close);

    std::vector<std::string> valid_urls;

    {
        std::shared_lock<std::shared_mutex> lock(this->cache_mutex_);

        if (this->cached_gateways_.empty()) {
            res.setStatusCode(HttpResponse::HttpStatusCode::K503ServiceUnavailable);
            res.setContentType("application/json");
            res.setBody(R"({"error": "No gateways available"})");

            return;
        }

        int count = std::min<std::size_t>(2, this->cached_gateways_.size());

        int index{0};
        while(valid_urls.size() < count && index < this->cached_gateways_.size()) {
            const std::string& gateway_url = this->cached_gateways_[index++].gateway_url;
            
            {
                std::shared_lock<std::shared_mutex> conns_lock(this->etcd_conns_mutex_);
                if(this->etcd_conns_.contains(gateway_url)) {
                    valid_urls.push_back(gateway_url);
                }
            }
        }
    }

    std::string res_json = R"({"code": 0, "urls": [)";
    for (size_t i = 0; i < valid_urls.size(); ++i) {
        res_json += R"("ws://)" + valid_urls[i] + R"(")";
        if (i < valid_urls.size() - 1) {
            res_json += ",";
        }
    }
    res_json += "]}";

    res.setContentType("application/json");
    res.setBody(res_json);
    sendRes(conn, res);
}

struct AuthLoginAwaiter {

    auth::AuthServer::Stub* auth_stub_;
    std::function<void()> callback_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        auto loop = muduo::net::EventLoop::getEventLoopOfCurrentThread(); 

        auto context = std::make_shared<grpc::ClientContext>();
        auto request = std::make_shared<auth::LoginRequest>();
        auto response = std::make_shared<auth::LoginResponse>();

        this->auth_stub_->async()->Login(context.get(), request.get(), response.get(), 
        [handle, loop, context, request, response, callback = std::move(callback_)] (grpc::Status ok) {
            if(ok.ok()) {
                callback();
                
                loop->runInLoop([handle] () {
                    handle.resume();
                });
            }
        });
    }

    void await_resume() {};
};

AuthLoginAwaiter async_authLogin_coro(auth::AuthServer::Stub* stub, const std::function<void()>& callback) {
    return AuthLoginAwaiter{stub, std::move(callback)};
}

DetachedTask dispatchServer::onLogin(const TcpConnectionPtr& conn, const HttpRequest& req) {
    bool close = req_is_close(req);
    HttpResponse res(close);

    co_await async_authLogin_coro(this->Authstub.get(), [] () {});
}

DetachedTask dispatchServer::onRegister(const TcpConnectionPtr& conn, const HttpRequest& req) {

}

DetachedTask dispatchServer::onjoinSession(const TcpConnectionPtr& conn, const HttpRequest& req) {

}

DetachedTask dispatchServer::oncreateSession(const TcpConnectionPtr& conn, const HttpRequest& req) {

}

void dispatchServer::start() {
    auto response = this->etcd_client_->ls(this->watch_prefix_).get();

    for(int i = 0; i < response.keys().size(); ++i) {
        const std::string& ip_port = response.value(i).as_string();

        {
            std::unique_lock<std::shared_mutex> lock(this->etcd_conns_mutex_);
            auto it = this->etcd_conns_.find(ip_port);
            if(it == this->etcd_conns_.end()) this->etcd_conns_.insert(ip_port);
        }
    }

    this->etcd_watcher_ = std::make_unique<etcd::Watcher>(
        this->etcd_url_,
        this->watch_prefix_,
        [this] (const etcd::Response& response) {
            this->onetcdWatcher(response);
        },
        true
    );

    this->sync_worker_ = std::thread([this] () {
        this->BackendSyncTask();
    });

    this->server_->GetAsync("/api/get_gateway", 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) -> DetachedTask {
        this->onDispatch(conn, req);
    });

    this->server_->GetAsync("/api/login", 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) -> DetachedTask {
        this->onLogin(conn, req);
    });

    this->server_->GetAsync("/api/reg", 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) -> DetachedTask {
        this->onRegister(conn, req);
    });

    this->server_->GetAsync("/api/joinsession", 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) -> DetachedTask {
        this->onjoinSession(conn, req);
    });

    this->server_->GetAsync("/api/createsession", 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) -> DetachedTask {
        this->oncreateSession(conn, req);
    });

    this->server_->start();
}
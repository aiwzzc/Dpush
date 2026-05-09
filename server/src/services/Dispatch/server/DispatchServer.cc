#include "DispatchServer.h"
#include "yyjson/JsonView.h"

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "chat_generated.h"
#include "fbs/http_codec.h"

#include "constants/RedisKey.h"

#include <coroutine>

using namespace pulse::constants;
using namespace pulse::protocol;

dispatchServer::dispatchServer(const std::string& etcd_url) :
etcd_url_(etcd_url), ServiceRegistryClient_(etcd_url) {

    muduo::Logger::setLogLevel(muduo::Logger::FATAL);

    this->server_ = std::make_unique<HttpServer>(muduo::net::InetAddress{"0.0.0.0", 5001}, "HttpServer", 6);

    sw::redis::ConnectionOptions connection_options;
    connection_options.host = "127.0.0.1";
    connection_options.port = 6379;
    connection_options.db = 1;

    sw::redis::ConnectionPoolOptions pool_options;
    pool_options.size = 3;

    this->redisPool_ = std::make_unique<sw::redis::Redis>(connection_options, pool_options);

    this->HttpApiRoutes_ = {
        {
            pulse::constants::http::DISPATCH_API_PATH,
            [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
                return this->onDispatch(conn, std::move(req));
            }
        },

        {
            pulse::constants::http::LOGIN_API_PATH,
            [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
                return this->onLogin(conn, std::move(req));
            }
        },

        {
            pulse::constants::http::REGISTER_API_PATH,
            [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
                return this->onRegister(conn, req);
            }
        },

        {
            pulse::constants::http::JOINSESSION_API_PATH,
            [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
                return this->onjoinSession(conn, req);
            }
        },

        {
            pulse::constants::http::CREATESESSION_API_PATH,
            [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
                return this->oncreateSession(conn, req);
            }
        }
    };
}

dispatchServer::~dispatchServer() {
    this->running_ = false;
    if(this->sync_worker_.joinable()) this->sync_worker_.join();
}

void dispatchServer::BackendSyncTask() {
    while(this->running_) {
        std::unordered_map<std::string, std::string> gateway_loads;
        this->redisPool_->hgetall(rediskey::GatewayLoadKey, std::inserter(gateway_loads, gateway_loads.begin()));

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

DetachedTask dispatchServer::onDispatch(TcpConnectionPtr conn, HttpRequest req) {
    auto connection_opt = req.getHeader("Connection");

    const std::string& connection = connection_opt.has_value() ? *(connection_opt.value()) : "";
    bool close = connection == "close" ||
        (req.version() == HttpRequest::Version::kHttp10 && connection != "Keep-Alive");
    HttpResponse res(close);

    std::vector<std::string> valid_urls;

    {
        std::shared_lock<std::shared_mutex> lock(this->cache_mutex_);

        if (this->cached_gateways_.empty()) {
            res.setStatusCode(HttpResponse::HttpStatusCode::K503ServiceUnavailable);
            res.setContentType(std::string(http::CONTENT_TYPE_JSON));
            res.setBody(R"({"error": "No gateways available"})");

            co_return;
        }

        int count = std::min<std::size_t>(2, this->cached_gateways_.size());

        int index{0};
        while(valid_urls.size() < count && index < this->cached_gateways_.size()) {
            const std::string& gateway_url = this->cached_gateways_[index++].gateway_url;

            auto gateway_endpoints = this->ServiceRegistryClient_.GetAllEndpoints(this->gateway_prefix_);
            if(gateway_endpoints.index_map_.contains(gateway_url)) {
                valid_urls.push_back(gateway_url);
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

    res.setContentType(std::string(http::CONTENT_TYPE_JSON));
    res.setBody(res_json);
    std::string output;
    res.appendToBuffer(output);

    conn->send(output);
    if (res.closeConnection()) {
        conn->shutdown();
    }
}

DetachedTask dispatchServer::onLogin(TcpConnectionPtr conn, HttpRequest req) {
    return GenericApiHandler<auth::LoginRequest, auth::LoginResponse>(
        conn, req, 
        this->auth_client_pool_, this->ServiceRegistryClient_, this->auth_prefix_,
        [] (const char* data) -> auth::LoginRequest {
            auto rootMsg = ChatApp::GetRootMessage(data);
            auto payload = rootMsg->payload_as_LoginHttpReqbodyPayload();

            std::string_view email(payload->email()->c_str(), payload->email()->size());
            std::string_view password(payload->password()->c_str(), payload->password()->size());

            auth::LoginRequest grpc_req;
            grpc_req.set_email(email);
            grpc_req.set_password(password);

            return grpc_req;
        },

        [] (std::shared_ptr<auth::AuthServer::Stub> stub, grpc::ClientContext* context, 
            auth::LoginRequest* grpc_req, auth::LoginResponse* grpc_res, auto cb) {
            stub->async()->Login(context, grpc_req, grpc_res, std::move(cb));
        },

        [this] (const TcpConnectionPtr& conn, HttpRequest& req, auth::LoginResponse* grpc_res) {
            fbs::LoginInfo info{grpc_res->code(), grpc_res->error_msg(), 
            grpc_res->token(), grpc_res->userid(), grpc_res->username()};

            this->sendHttpResponse(conn, info, req, fbs::httpfbsCodec::BuildLoginResfbs);
        }
    );
}

DetachedTask dispatchServer::onRegister(TcpConnectionPtr conn, HttpRequest req) {
    return GenericApiHandler<auth::RegisterRequest, auth::RegisterResponse>(
        conn, req, 
        this->auth_client_pool_, this->ServiceRegistryClient_, this->auth_prefix_,
        [] (const char* data) -> auth::RegisterRequest {
            auto rootMsg = ChatApp::GetRootMessage(data);
            auto payload = rootMsg->payload_as_RegisterHttpReqbodyPayload();

            std::string_view email(payload->email()->c_str(), payload->email()->size());
            std::string_view username(payload->username()->c_str(), payload->username()->size());
            std::string_view password(payload->password()->c_str(), payload->password()->size());

            auth::RegisterRequest grpc_req;
            grpc_req.set_email(email);
            grpc_req.set_username(username);
            grpc_req.set_password(password);

            return grpc_req;
        },

        [] (std::shared_ptr<auth::AuthServer::Stub> stub, grpc::ClientContext* context, 
        auth::RegisterRequest* grpc_req, auth::RegisterResponse* grpc_res, auto cb) {
            stub->async()->Register(context, grpc_req, grpc_res, std::move(cb));
        },

        [this] (const TcpConnectionPtr& conn, HttpRequest& req, auth::RegisterResponse* grpc_res) {
            fbs::RegisterInfo info{grpc_res->code(), grpc_res->error_msg()};
            this->sendHttpResponse(conn, info, req, fbs::httpfbsCodec::BuildRegisterResfbs);
        }
    );
}

DetachedTask dispatchServer::onjoinSession(TcpConnectionPtr conn, HttpRequest req) {
    return GenericApiHandler<logic::joinSessionRequest, logic::joinSessionResponse>(
        conn, req, 
        this->logic_client_pool_, this->ServiceRegistryClient_, this->logic_prefix_,
        [] (const char* data) -> logic::joinSessionRequest {
            auto rootMsg = ChatApp::GetRootMessage(data);
            auto payload = rootMsg->payload_as_JoinSessionHttpReqbodyPayload();

            int64_t userid = payload->userid();
            std::string_view roomname(payload->room_name()->c_str(), payload->room_name()->size());

            logic::joinSessionRequest grpc_req;
            grpc_req.set_userid(userid);
            grpc_req.set_roomname(roomname);

            return grpc_req;
        },

        [] (std::shared_ptr<logic::LogicServer::Stub> stub, grpc::ClientContext* context, 
        logic::joinSessionRequest* grpc_req, logic::joinSessionResponse* grpc_res, auto cb) {
            stub->async()->joinSession(context, grpc_req, grpc_res, std::move(cb));
        },

        [this] (const TcpConnectionPtr& conn, HttpRequest& req, logic::joinSessionResponse* grpc_res) {
            fbs::JoinSessionInfo info{grpc_res->code(), grpc_res->error_msg(), 
            grpc_res->userid(), grpc_res->roomid()};
            this->sendHttpResponse(conn, info, req, fbs::httpfbsCodec::BuildJoinSessionResfbs);
        }
    );
}

DetachedTask dispatchServer::oncreateSession(TcpConnectionPtr conn, HttpRequest req) {
    return GenericApiHandler<logic::createSessionRequest, logic::createSessionResponse>(
        conn, req, 
        this->logic_client_pool_, this->ServiceRegistryClient_, this->logic_prefix_,
        [] (const char* data) -> logic::createSessionRequest {
            auto rootMsg = ChatApp::GetRootMessage(data);
            auto payload = rootMsg->payload_as_createSessionHttpReqbodyPayload();

            int64_t userid = payload->userid();
            std::string_view roomname{payload->room_name()->c_str(), payload->room_name()->size()};

            logic::createSessionRequest grpc_req;
            grpc_req.set_userid(userid);
            grpc_req.set_roomname(roomname);

            return grpc_req;
        },

        [] (std::shared_ptr<logic::LogicServer::Stub> stub, grpc::ClientContext* context, 
        logic::createSessionRequest* grpc_req, logic::createSessionResponse* grpc_res, auto cb) {
            stub->async()->createSession(context, grpc_req, grpc_res, std::move(cb));
        },

        [this] (const TcpConnectionPtr& conn, HttpRequest& req, logic::createSessionResponse* grpc_res) {
            fbs::CreateSessionInfo info{grpc_res->code(), grpc_res->error_msg(), 
            grpc_res->userid(), grpc_res->roomid()};
            this->sendHttpResponse(conn, info, req, fbs::httpfbsCodec::BuildCreateSessionResfbs);
        }
    );
}

void dispatchServer::start() {

    this->ServiceRegistryClient_.RegisterSelf("/services/dispatch/", "192.168.183.130:5001");

    this->ServiceRegistryClient_.Subscribe(this->gateway_prefix_, 
    [] (const etcd::Event& event) {});

    this->auth_client_pool_.Init(this->ServiceRegistryClient_, this->auth_prefix_);
    this->logic_client_pool_.Init(this->ServiceRegistryClient_, this->logic_prefix_);

    this->sync_worker_ = std::thread([this] () {
        this->BackendSyncTask();
    });

    for(auto& [path, handle] : this->HttpApiRoutes_) {
        this->server_->GetAsync(path, std::move(handle));
    }

    this->server_->start();
}
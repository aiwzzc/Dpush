#include "DispatchServer.h"
#include "yyjson/JsonView.h"

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "chat_generated.h"

#include "constants/RedisKey.h"
#include "constants/http_constants.h"

#include <coroutine>

using namespace pulse::constants;

dispatchServer::dispatchServer(const std::string& etcd_url) :
etcd_url_(etcd_url), ServiceRegistryClient_(etcd_url) {

    muduo::Logger::setLogLevel(muduo::Logger::FATAL);

    // this->etcd_client_ = std::make_shared<etcd::Client>(this->etcd_url_);
    this->server_ = std::make_unique<HttpServer>(muduo::net::InetAddress{"0.0.0.0", 5001}, "HttpServer", 6);

    // this->Authchannel = grpc::CreateChannel("127.0.0.1:5006", grpc::InsecureChannelCredentials());
    // this->Authstub = auth::AuthServer::NewStub(this->Authchannel);

    // this->Logicchannel = grpc::CreateChannel("127.0.0.1:5008", grpc::InsecureChannelCredentials());
    // this->Logicstub = logic::LogicServer::NewStub(this->Logicchannel);

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

namespace {

bool req_is_close(const HttpRequest& req) {
    auto connection_opt = req.getHeader("Connection");

    const std::string& connection = connection_opt.has_value() ? *(connection_opt.value()) : "";
    bool close = connection == "close" ||
        (req.version() == HttpRequest::Version::kHttp10 && connection != "Keep-Alive");

    return close;
}

void fill_http_response(int info_code, HttpResponse& res) {
    if(info_code < 0) {
        res.setStatusCode(HttpResponse::HttpStatusCode::k400BadRequest);
        res.setStatusMessage(http::MSG_BAD_REQUEST);
        res.setCloseConnection(true);

    } else {
        res.setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
        res.setStatusMessage(http::MSG_OK);
        res.setCloseConnection(true);
    }

    res.setContentType(std::string(http::CONTENT_TYPE_OCTET));
}

void sendRes(const TcpConnectionPtr& conn, HttpResponse& res) {
    std::string output;
    res.appendToBuffer(output);

    conn->send(output);
    if (res.closeConnection()) {
        conn->shutdown();
    }
}

template<typename Info, typename Builder>
void sendHttpResponse(const TcpConnectionPtr& conn, const Info& info, HttpRequest& req, Builder&& builder) {
    bool close = req_is_close(req);
    HttpResponse res(close);

    fill_http_response(info.errcode, res);

    res.setBody(builder(info));

    sendRes(conn, res);
}

};

DetachedTask dispatchServer::onDispatch(TcpConnectionPtr conn, HttpRequest req) {
    bool close = req_is_close(req);
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
            
            // {
            //     std::shared_lock<std::shared_mutex> conns_lock(this->etcd_conns_mutex_);
            //     if(this->etcd_conns_.contains(gateway_url)) {
            //         valid_urls.push_back(gateway_url);
            //     }
            // }
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
    sendRes(conn, res);
}

namespace {

struct AuthLoginAwaiter {

    auth::AuthServer::Stub* auth_stub_;
    const std::string& body_;
    LoginInfo logininfo_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        auto loop = muduo::net::EventLoop::getEventLoopOfCurrentThread(); 

        auto rootMsg = ChatApp::GetRootMessage(this->body_.data());
        auto payload = rootMsg->payload_as_LoginHttpReqbodyPayload();

        std::string_view email(payload->email()->c_str(), payload->email()->size());
        std::string_view password(payload->password()->c_str(), payload->password()->size());

        auto context = std::make_shared<grpc::ClientContext>();
        auto request = std::make_shared<auth::LoginRequest>();
        auto response = std::make_shared<auth::LoginResponse>();

        request->set_email(email);
        request->set_password(password);

        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        context->set_deadline(deadline);

        this->auth_stub_->async()->Login(context.get(), request.get(), response.get(), 
        [handle, loop, context, request, response, this] (grpc::Status s) {
            if(s.ok()) {
                this->logininfo_ = LoginInfo{response->code(), response->error_msg(), 
                response->token(), response->userid(), response->username()};
                
                loop->runInLoop([handle] () {
                    handle.resume();
                });
            }
        });
    }

    LoginInfo await_resume() { return std::move(this->logininfo_); };
};

AuthLoginAwaiter async_authLogin_coro(auth::AuthServer::Stub* stub, const std::string& body) {
    return AuthLoginAwaiter{stub, body, {}};
}

std::string BuildLoginResfbs(const LoginInfo& info) {
    thread_local flatbuffers::FlatBufferBuilder builder(128);
    builder.Clear();

    auto err_msg_offset = builder.CreateString(info.errmsg);
    auto username_offset = builder.CreateString(info.username);
    auto token_offset = builder.CreateString(info.token);

    ChatApp::LoginHttpResbodyPayloadBuilder resBuilder(builder);
    resBuilder.add_code(info.errcode);
    resBuilder.add_err_msg(err_msg_offset);
    resBuilder.add_userid(info.userid);
    resBuilder.add_username(username_offset);
    resBuilder.add_token(token_offset);
    auto loginResOffset = resBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_LoginHttpResbodyPayload);
    rootMsgBuilder.add_payload(loginResOffset.Union());
    auto rootMsgOffset = rootMsgBuilder.Finish();

    builder.Finish(rootMsgOffset);
    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int size = builder.GetSize();

    return std::string(data, size);
}

};

DetachedTask dispatchServer::onLogin(TcpConnectionPtr conn, HttpRequest req) {
    std::string auth_endpoint = this->ServiceRegistryClient_.GetEndpoint(this->auth_prefix_);
    auth::AuthServer::Stub* stub = nullptr;

    {
        std::shared_lock<std::shared_mutex> lock(this->auth_stubs_mutex_);
        stub = this->auth_stubs_[auth_endpoint].get();
    }

    if(stub == nullptr) co_return;

    LoginInfo info = co_await async_authLogin_coro(stub, req.body());

    sendHttpResponse(conn, info, req, BuildLoginResfbs);
}

namespace {

struct AuthRegisterAwaiter {

    auth::AuthServer::Stub* auth_stub_;
    const std::string& body_;
    RegisterInfo info_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {

        auto loop = muduo::net::EventLoop::getEventLoopOfCurrentThread();

        auto rootMsg = ChatApp::GetRootMessage(this->body_.data());
        auto payload = rootMsg->payload_as_RegisterHttpReqbodyPayload();

        std::string_view username(payload->username()->c_str(), payload->username()->size());
        std::string_view email(payload->email()->c_str(), payload->email()->size());
        std::string_view password(payload->password()->c_str(), payload->password()->size());

        auto context = std::make_shared<grpc::ClientContext>();
        auto request = std::make_shared<auth::RegisterRequest>();
        auto response = std::make_shared<auth::RegisterResponse>();

        request->set_email(email);
        request->set_username(username);
        request->set_password(password);

        this->auth_stub_->async()->Register(context.get(), request.get(), response.get(), 
        [this, handle, loop, request, response, context] (grpc::Status s) {
            if(s.ok()) {
                this->info_ = RegisterInfo{response->code(), response->error_msg()};

                loop->runInLoop([handle] () {
                    handle.resume();
                });
            }
        });

    };

    RegisterInfo await_resume() { return std::move(this->info_); }

};

AuthRegisterAwaiter async_authregister_coro(auth::AuthServer::Stub* stub, const std::string& body) {
    return AuthRegisterAwaiter{stub, body, {}};
}

std::string BuildRegisterResfbs(const RegisterInfo& info) {
    thread_local flatbuffers::FlatBufferBuilder builder(128);
    builder.Clear();

    auto err_msg_offset = builder.CreateString(info.errmsg);

    ChatApp::RegisterHttpResbodyPayloadBuilder resBuilder(builder);
    resBuilder.add_code(info.errcode);
    resBuilder.add_err_msg(err_msg_offset);
    auto resOffset = resBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_RegisterHttpResbodyPayload);
    rootMsgBuilder.add_payload(resOffset.Union());
    auto rootMsgOffset = rootMsgBuilder.Finish();

    builder.Finish(rootMsgOffset);
    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int size = builder.GetSize();

    return std::string(data, size);
}

};

DetachedTask dispatchServer::onRegister(TcpConnectionPtr conn, HttpRequest req) {
    std::string auth_endpoint = this->ServiceRegistryClient_.GetEndpoint(this->auth_prefix_);
    auth::AuthServer::Stub* stub = nullptr;

    {
        std::shared_lock<std::shared_mutex> lock(this->auth_stubs_mutex_);
        stub = this->auth_stubs_[auth_endpoint].get();
    }

    if(stub == nullptr) co_return;

    RegisterInfo info = co_await async_authregister_coro(stub, req.body());

    sendHttpResponse(conn, info, req, BuildRegisterResfbs);
}

namespace {

struct LogicJoinSessionAwaiter {

    logic::LogicServer::Stub* logic_stub_;
    const std::string& body_;
    JoinSessionInfo info_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {

        auto rootMsg = ChatApp::GetRootMessage(this->body_.data());
        auto payload = rootMsg->payload_as_JoinSessionHttpReqbodyPayload();

        int64_t userid = payload->userid();
        std::string_view roomname(payload->room_name()->c_str(), payload->room_name()->size());

        auto context = std::make_shared<grpc::ClientContext>();
        auto request = std::make_shared<logic::joinSessionRequest>();
        auto response = std::make_shared<logic::joinSessionResponse>();

        request->set_userid(userid);
        request->set_roomname(roomname);

        auto loop = muduo::net::EventLoop::getEventLoopOfCurrentThread();
        
        this->logic_stub_->async()->joinSession(context.get(), request.get(), response.get(), 
        [this, handle, request, context, response, loop] (grpc::Status s) {
            if(s.ok()) {
                this->info_ = JoinSessionInfo{response->code(), response->error_msg(), 
                    request->userid(), response->roomid()};

                loop->runInLoop([handle] () {
                    handle.resume();
                });
            }
        });
    }

    JoinSessionInfo await_resume() { return std::move(this->info_); }

};

LogicJoinSessionAwaiter async_joinSession_coro(logic::LogicServer::Stub* stub, const std::string& body) {
    return LogicJoinSessionAwaiter{stub, body, {}};
}

std::string BuildJoinSessionResfbs(const JoinSessionInfo& info) {
    thread_local flatbuffers::FlatBufferBuilder builder(128);
    builder.Clear();

    auto roomid_offset = builder.CreateString(std::to_string(info.room_id));
    auto err_msg_offset = builder.CreateString(info.errmsg);

    ChatApp::JoinSessionHttpResbodyPayloadBuilder resBuilder(builder);
    resBuilder.add_code(info.errcode);
    resBuilder.add_err_msg(err_msg_offset);
    resBuilder.add_userid(info.userid);
    resBuilder.add_room_id(roomid_offset);
    auto resOffset = resBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_JoinSessionHttpResbodyPayload);
    rootMsgBuilder.add_payload(resOffset.Union());
    auto rootMsgOffset = rootMsgBuilder.Finish();

    builder.Finish(rootMsgOffset);

    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int size = builder.GetSize();

    return std::string(data, size);
}
    
};

DetachedTask dispatchServer::onjoinSession(TcpConnectionPtr conn, HttpRequest req) {
    std::string logic_endpoint = this->ServiceRegistryClient_.GetEndpoint(this->logic_prefix_);

    logic::LogicServer::Stub* stub = nullptr;

    {
        std::shared_lock<std::shared_mutex> lock(this->logic_stubs_mutex_);
        stub = this->logic_stubs_[logic_endpoint].get();
    }

    if(stub == nullptr) co_return;

    JoinSessionInfo info = co_await async_joinSession_coro(stub, req.body());

    sendHttpResponse(conn, info, req, BuildJoinSessionResfbs);
}

namespace {

struct LogicCreateSessionAwaiter {

    logic::LogicServer::Stub* logic_stub_;
    const std::string& body_;
    CreateSessionInfo info_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {

        auto rootMsg = ChatApp::GetRootMessage(this->body_.data());
        auto payload = rootMsg->payload_as_createSessionHttpReqbodyPayload();

        int64_t userid = payload->userid();
        std::string_view roomname{payload->room_name()->c_str(), payload->room_name()->size()};

        auto context = std::make_shared<grpc::ClientContext>();
        auto request = std::make_shared<logic::createSessionRequest>();
        auto response = std::make_shared<logic::createSessionResponse>();

        request->set_userid(userid);
        request->set_roomname(roomname);

        auto loop = muduo::net::EventLoop::getEventLoopOfCurrentThread();

        this->logic_stub_->async()->createSession(context.get(), request.get(), response.get(), 
        [this, handle, loop, context, request, response] (grpc::Status s) {
            if(s.ok()) {
                this->info_ = CreateSessionInfo{response->code(), response->error_msg(), 
                    request->userid(), response->roomid()};

                loop->runInLoop([handle] () {
                    handle.resume();
                });
            }
        });
    }

    CreateSessionInfo await_resume() { return std::move(this->info_); }

};

LogicCreateSessionAwaiter async_createSession_coro(logic::LogicServer::Stub* stub, const std::string& body) {
    return LogicCreateSessionAwaiter{stub, body, {}};
}

std::string BuildCreateSessionResfbs(const CreateSessionInfo& info) {
    thread_local flatbuffers::FlatBufferBuilder builder(128);
    builder.Clear();

    auto roomid_offset = builder.CreateString(std::to_string(info.room_id));
    auto err_msg_offset = builder.CreateString(info.errmsg);

    ChatApp::createSessionHttpResbodyPayloadBuilder resBuilder(builder);
    resBuilder.add_code(info.errcode);
    resBuilder.add_err_msg(err_msg_offset);
    resBuilder.add_userid(info.userid);
    resBuilder.add_room_id(roomid_offset);
    auto resOffset = resBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_createSessionHttpResbodyPayload);
    rootMsgBuilder.add_payload(resOffset.Union());
    auto rootMsgOffset = rootMsgBuilder.Finish();

    builder.Finish(rootMsgOffset);

    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int size = builder.GetSize();

    return std::string(data, size);
}

};

DetachedTask dispatchServer::oncreateSession(TcpConnectionPtr conn, HttpRequest req) {
    std::string logic_endpoint = this->ServiceRegistryClient_.GetEndpoint(this->logic_prefix_);

    logic::LogicServer::Stub* stub = nullptr;

    {
        std::shared_lock<std::shared_mutex> lock(this->logic_stubs_mutex_);
        stub = this->logic_stubs_[logic_endpoint].get();
    }

    if(stub == nullptr) co_return;
   
    CreateSessionInfo info = co_await async_createSession_coro(stub, req.body());

    sendHttpResponse(conn, info, req, BuildCreateSessionResfbs);
}

void dispatchServer::start() {

    this->ServiceRegistryClient_.RegisterSelf("/services/dispatch/", "192.168.183.130:5001");

    this->ServiceRegistryClient_.Subscribe(this->gateway_prefix_, 
    [] (const etcd::Event& event) {});

    this->ServiceRegistryClient_.Subscribe(this->auth_prefix_, 
    [this] (const etcd::Event& event) {
        this->onWatcher(event, this->auth_prefix_, 
        this->auth_stubs_, this->auth_stubs_mutex_, [] (auto channel) {
            return  auth::AuthServer::NewStub(channel);
        });
    });

    this->ServiceRegistryClient_.Subscribe(this->logic_prefix_, 
    [this] (const etcd::Event& event) {
        this->onWatcher(event, this->logic_prefix_, 
        this->logic_stubs_, this->logic_stubs_mutex_, [] (auto channel) {
            return logic::LogicServer::NewStub(channel);
        });
    });

    std::vector<std::string> auths = this->ServiceRegistryClient_.GetAllEndpoints(this->auth_prefix_).endpoints_;
    for(const std::string& auth : auths) {
        this->addStub(this->auth_stubs_, this->auth_stubs_mutex_, auth, [] (auto channel) {
            return auth::AuthServer::NewStub(channel);
        });
    }

    std::vector<std::string> logics = this->ServiceRegistryClient_.GetAllEndpoints(this->logic_prefix_).endpoints_;
    for(const std::string& logic : logics) {
        this->addStub(this->logic_stubs_, this->logic_stubs_mutex_, logic, [] (auto channel) {
            return logic::LogicServer::NewStub(channel);
        });
    }

    // auto response = this->etcd_client_->ls(this->watch_prefix_).get();

    // for(int i = 0; i < response.keys().size(); ++i) {
    //     const std::string& ip_port = response.value(i).as_string();

    //     {
    //         std::unique_lock<std::shared_mutex> lock(this->etcd_conns_mutex_);
    //         auto it = this->etcd_conns_.find(ip_port);
    //         if(it == this->etcd_conns_.end()) this->etcd_conns_.insert(ip_port);
    //     }
    // }

    // this->etcd_watcher_ = std::make_unique<etcd::Watcher>(
    //     this->etcd_url_,
    //     this->watch_prefix_,
    //     [this] (const etcd::Response& response) {
    //         this->onetcdWatcher(response);
    //     },
    //     true
    // );

    this->sync_worker_ = std::thread([this] () {
        this->BackendSyncTask();
    });

    this->server_->GetAsync(http::DISPATCH_API_PATH, 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
        return this->onDispatch(conn, std::move(req));
    });

    this->server_->GetAsync(http::LOGIN_API_PATH, 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
        return this->onLogin(conn, std::move(req));
    });

    this->server_->GetAsync(http::REGISTER_API_PATH, 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
        return this->onRegister(conn, req);
    });

    this->server_->GetAsync(http::JOINSESSION_API_PATH, 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
        return this->onjoinSession(conn, req);
    });

    this->server_->GetAsync(http::CREATESESSION_API_PATH, 
    [this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
        return this->oncreateSession(conn, req);
    });

    this->server_->start();
}
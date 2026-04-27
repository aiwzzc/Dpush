#include "grpcClient.h"

#include "httpServer/HttpResponse.h"
#include "httpServer/HttpRequest.h"

#include "yyjson/JsonView.h"

std::string grpcClient::api_error_id_to_string(api_error_id id) {
    std::string res;
    switch(id) {
        case api_error_id::bad_request:
            res.assign("bad_request");
            break;
        
        case api_error_id::email_exists:
            res.assign("email_exists");
            break;

        case api_error_id::login_failed_email_error:
            res.assign("login_failed_email_error");
            break;

        case api_error_id::login_failed_password_error:
            res.assign("login_failed_password_error");
            break;

        case api_error_id::username_exists:
            res.assign("username_exists");
            break;
        
        default:
            break;
    }

    return res;
}

std::optional<grpcClient::api_error_id> grpcClient::to_api_error_id(int v) {
    switch (v) {
        case -6: return api_error_id::bad_request;
        case -5: return api_error_id::login_failed_password_error;
        case -4: return api_error_id::login_failed_email_error;
        case -3: return api_error_id::email_exists;
        case -2: return api_error_id::username_exists;
        case -1: return api_error_id::Unknown_error;
        default: return std::nullopt;
    }
}

void grpcClient::rpcLoginAsync(const HttpRequest& req, int& errcode, std::string& errmsg, 
    std::function<void(LogicInfo)> callback) {

    auto context = std::make_shared<ClientContext>();
    auto request = std::make_shared<auth::LoginRequest>();
    auto response = std::make_shared<auth::LoginResponse>();

    JsonDoc root;

    if (!root.parse(req.body().c_str(), req.body().size())) {
        errcode = -6;
        errmsg.assign("JSON格式错误");
        return;
    }

    if (!root.root().isMember("email")    || !root.root()["email"].isString() ||
        !root.root().isMember("password") || !root.root()["password"].isString()) {
        errcode = -6;
        errmsg.assign("请求参数不全");
        return;
    }

    request->set_email(root.root()["email"].asString());
    request->set_password(root.root()["password"].asString());

    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context->set_deadline(deadline);
    
    this->Authstub->async()->Login(context.get(), request.get(), response.get(), 
    [request, response, context, callback] (grpc::Status s) {
        if(s.ok()) {
            LogicInfo info = LogicInfo{response->code(), response->error_msg(), 
            response->token(), response->userid(), response->username()};
            callback(info);
        }
    });
}

void grpcClient::rpcRegisterAsync(const HttpRequest& req, int& errcode, std::string& errmsg, 
    std::function<void(RegisterInfo)> callback) {

    auto context = std::make_shared<ClientContext>();
    auto request = std::make_shared<auth::RegisterRequest>();
    auto response = std::make_shared<auth::RegisterResponse>();

    JsonDoc root;

    if (!root.parse(req.body().c_str(), req.body().size())) {
        errcode = -6;
        errmsg.assign("JSON格式错误");
        return;
    }

    if (!root.root().isMember("username") || !root.root()["username"].isString() ||
        !root.root().isMember("email")    || !root.root()["email"].isString() ||
        !root.root().isMember("password") || !root.root()["password"].isString()) {
        errcode = -6;
        errmsg.assign("请求参数不全");
        return;
    }

    request->set_username(root.root()["username"].asString());
    request->set_email(root.root()["email"].asString());
    request->set_password(root.root()["password"].asString());

    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context->set_deadline(deadline);

    this->Authstub->async()->Register(context.get(), request.get(), response.get(), 
    [request, response, context, callback] (grpc::Status s) {
        if(s.ok()) {
            RegisterInfo info = RegisterInfo{response->code(), response->error_msg()};
            callback(info);
        }
    });
}

void grpcClient::rpcCilentMessageAsync(const std::string& message, int32_t userid, std::string username,
    std::function<void(std::string)> callback) {

    auto request = std::make_shared<logic::clientMessageRequest>();

    request->set_message(std::move(message));
    request->set_userid(userid);
    request->set_username(username);

    auto response = std::make_shared<logic::clientMessageResponse>();
    auto context = std::make_shared<ClientContext>();

    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context->set_deadline(deadline);

    this->Logicstub->async()->clientMessage(context.get(), request.get(), response.get(), 
        [request, response, context, callback] (grpc::Status s) {
            if(s.ok() && !response->message().empty()) {
                callback(response->message());

            } else {
                callback("");
            }
        });
}

void grpcClient::rpcclearCursorsAsync(int32_t userid, std::function<void()> callback) {

    auto context = std::make_shared<ClientContext>();
    auto request = std::make_shared<logic::clearCursorsRequest>();
    auto response = std::make_shared<logic::clearCursorsResponse>();

    request->set_userid(userid);

    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context->set_deadline(deadline);

    this->Logicstub->async()->clearCursors(context.get(), request.get(), response.get(), 
    [context, request, response, callback] (grpc::Status s) {
        if(s.ok()) callback();
    });
}

void grpcClient::rpcGetUserRoomListAsync(int32_t userid, const std::function<void(std::vector<std::string>&)>& callback) {

    auto context = std::make_shared<ClientContext>();
    auto request = std::make_shared<room::GetUserRoomListRequest>();
    auto response = std::make_shared<room::GetUserRoomListResponse>();

    request->set_userid(userid);

    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context->set_deadline(deadline);

    this->RoomStub->async()->GetUserRoomList(context.get(), request.get(), response.get(), 
    [request, response, context, callback = std::move(callback)] (grpc::Status s) {
        if(s.ok()) {
            std::vector<std::string> roomlist;
    
            for(const auto& roominfo : response->roomlist()) {
                std::string roomid = roominfo.room_id();

                roomlist.emplace_back(roomid);
            }
        
            callback(roomlist);
        }
    });
}

void grpcClient::rpcJoinSessionAsync(int32_t userid, const std::string& roomname, const std::function<void(int, const std::string&, int64_t)>& cb) {
    auto ctx = std::make_shared<ClientContext>();
    auto request = std::make_shared<logic::joinSessionRequest>();
    auto response = std::make_shared<logic::joinSessionResponse>();

    request->set_userid(userid);
    request->set_roomname(roomname);

    this->Logicstub->async()->joinSession(ctx.get(), request.get(), response.get(), 
    [ctx, request, response, cb = std::move(cb)] (grpc::Status s) {
        if(s.ok()) {
            cb(response->code(), response->error_msg(), response->roomid());
        }
    });
}

void grpcClient::rpcCreateSessionAsync(int32_t userid, const std::string& roomname, const std::function<void(int32_t, const std::string&, int64_t)>& cb) {
    auto ctx = std::make_shared<ClientContext>();
    auto request = std::make_shared<logic::createSessionRequest>();
    auto response = std::make_shared<logic::createSessionResponse>();

    request->set_userid(userid);
    request->set_roomname(roomname);

    this->Logicstub->async()->createSession(ctx.get(), request.get(), response.get(), 
    [ctx, request, response, cb = std::move(cb)] (grpc::Status s) {
        cb(response->code(), response->error_msg(), response->roomid());
    });
}

void grpcClient::rpcJoinRooms(int32_t userid, std::vector<std::string>& rooms) {
    ClientContext ctx;
    room::JoinRoomResponse response;

    auto writer = this->RoomStub->JoinRooms(&ctx, &response);

    for(auto& room : rooms) {
        room::JoinRoomRequest req;
        req.set_userid(userid);
        req.set_room_id(room);

        writer->Write(req);
    }

    writer->WritesDone();
    grpc::Status status = writer->Finish();
}

BathPullClientReactor::BathPullClientReactor(logic::LogicServer::Stub* stub, 
    const std::string& msg,
    const std::function<void(const std::string&)>& onmessage, 
    const std::function<void(const ::grpc::Status&)>& ondone) : 
    on_message_cb_(std::move(onmessage)), on_done_cb_(std::move(ondone)) {

    this->request_.set_message(msg);

    stub->async()->bathPullMessage(&this->context_, &this->request_, this);
    StartRead(&this->response_);
    StartCall();
}

void BathPullClientReactor::OnReadDone(bool ok) {
    if(ok) {
        if(this->on_message_cb_) {
            on_message_cb_(this->response_.message());
        }

        StartRead(&this->response_);

    } else {

    }
}

void BathPullClientReactor::OnDone(const ::grpc::Status& status) {
    if(this->on_done_cb_) {
        this->on_done_cb_(status);
    }

    delete this;
}

void grpcClient::rpcBathPullMessageAsync(const std::string& message, std::function<void(const std::string&)> callback) {

    new BathPullClientReactor(this->Logicstub.get(), message, std::move(callback), 
    [] (const ::grpc::Status& status) {
        if(status.ok()) {

        }
    });
}

void grpcClient::rpcIsSubSessionAsync(int32_t userid, std::string& room_id, const std::function<void(const std::string&)>& callback) {
    auto ctx = std::make_shared<ClientContext>();
    auto request = std::make_shared<room::IsSubRoomRequest>();
    auto response = std::make_shared<room::IsSubRoomResponse>();

    request->set_userid(userid);
    request->set_room_id(room_id);

    this->RoomStub->async()->IsSubRoom(ctx.get(), request.get(), response.get(), 
    [ctx, request, response, callback = std::move(callback)] (grpc::Status s) {
        if(s.ok()) {
            callback(response->message());
        }
    });
}

void grpcClient::rpcPullMessageAsync(int64_t roomid, std::string& roomname, const std::function<void(const std::string&)>& callback) {
    auto ctx = std::make_shared<ClientContext>();
    auto request = std::make_shared<logic::PullMessageRequest>();
    auto response = std::make_shared<logic::PullMessageResponse>();

    request->set_roomid(roomid);
    request->set_roomname(roomname);
    request->set_messageid(0);

    this->Logicstub->async()->pullMessage(ctx.get(), request.get(), response.get(),
    [ctx, request, response, callback = std::move(callback)] (grpc::Status s) {
        callback(response->message());
    });
}
#include "grpcClient.h"

#include "httpServer/HttpResponse.h"
#include "httpServer/HttpRequest.h"

#include <jsoncpp/json/json.h>

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

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(req.body(), root)) {
        errcode = -6;
        errmsg.assign("JSON格式错误");
        return;
    }

    if (!root.isMember("email")    || !root["email"].isString() ||
        !root.isMember("password") || !root["password"].isString()) {
        errcode = -6;
        errmsg.assign("请求参数不全");
        return;
    }

    request->set_email(root["email"].asString());
    request->set_password(root["password"].asString());
    
    this->Authstub->async()->Login(context.get(), request.get(), response.get(), 
    [request, response, context, callback] (grpc::Status s) {
        if(s.ok()) {
            LogicInfo info = LogicInfo{response->code(), response->error_msg(), response->token()};
            callback(info);
        }
    });
}

void grpcClient::rpcRegisterAsync(const HttpRequest& req, int& errcode, std::string& errmsg, 
    std::function<void(RegisterInfo)> callback) {

    auto context = std::make_shared<ClientContext>();
    auto request = std::make_shared<auth::RegisterRequest>();
    auto response = std::make_shared<auth::RegisterResponse>();

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(req.body(), root)) {
        errcode = -6;
        errmsg.assign("JSON格式错误");
        return;
    }

    if (!root.isMember("username") || !root["username"].isString() ||
        !root.isMember("email")    || !root["email"].isString() ||
        !root.isMember("password") || !root["password"].isString()) {
        errcode = -6;
        errmsg.assign("请求参数不全");
        return;
    }

    request->set_username(root["username"].asString());
    request->set_email(root["email"].asString());
    request->set_password(root["password"].asString());

    this->Authstub->async()->Register(context.get(), request.get(), response.get(), 
    [request, response, context, callback] (grpc::Status s) {
        if(s.ok()) {
            RegisterInfo info = RegisterInfo{response->code(), response->error_msg()};
            callback(info);
        }
    });
}

void grpcClient::rpcinitialPullMessageAsync(int32_t userid, std::string username, const int messagecount,
    std::function<void(std::string)> callback) {

    auto request = std::make_shared<logic::pullMessageRequest>();
    auto response = std::make_shared<logic::pullMessageResponse>();
    auto context = std::make_shared<ClientContext>();

    request->set_userid(userid);
    request->set_username(std::move(username));
    request->set_messagecount(messagecount);

    this->Logicstub->async()->initialPullMessage(context.get(), request.get(), response.get(),
    [request, response, context, callback] (grpc::Status s) {
        if(s.ok() && !response->message().empty()) {
            callback(response->message());

        } else {
            callback("");
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

    this->Logicstub->async()->clearCursors(context.get(), request.get(), response.get(), 
    [context, request, response, callback] (grpc::Status s) {
        if(s.ok()) callback();
    });
}

void grpcClient::rpcGetUserRoomListAsync(int32_t userid, std::function<void(std::vector<std::string>)> callback) {

    auto context = std::make_shared<ClientContext>();
    auto request = std::make_shared<room::GetUserRoomListRequest>();
    auto response = std::make_shared<room::GetUserRoomListResponse>();

    request->set_userid(userid);

    this->RoomStub->async()->GetUserRoomList(context.get(), request.get(), response.get(), 
    [request, response, context, callback] (grpc::Status s) {
        if(s.ok()) {
            std::vector<std::string> roomlist;
    
            for(const auto& roominfo : response->roomlist()) {
                std::string roomid = roominfo.room_id();
                std::size_t colon_pos = roomid.find(":");
                if(colon_pos == std::string::npos) return ;

                std::string real_roomid = roomid.substr(0, colon_pos);
                roomlist.emplace_back(real_roomid);
            }
        
            callback(roomlist);
        }
    });
    
}
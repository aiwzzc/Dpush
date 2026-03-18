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


void grpcClient::rpcLogin(const HttpRequest& req, HttpResponse& res, int& errcode, std::string& errmsg) {
    ClientContext ctx;
    auth::LoginRequest request;
    auth::LoginResponse response;

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

    request.set_email(root["email"].asString());
    request.set_password(root["password"].asString());
    
    Status s = this->Authstub->Login(&ctx, request, &response);
    if(s.ok()) {
        if(response.code() < 0) {
            errcode = response.code();
            errmsg.assign(response.error_msg());

            return;
        }
        
        errcode = 1;
        res.setCookie(response.token());
    }
}

void grpcClient::rpcRegister(const HttpRequest& req, HttpResponse& res, int& errcode, std::string& errmsg) {
    ClientContext ctx;
    auth::RegisterRequest request;
    auth::RegisterResponse response;

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

    request.set_username(root["username"].asString());
    request.set_email(root["email"].asString());
    request.set_password(root["password"].asString());

    Status s = this->Authstub->Register(&ctx, request, &response);
    if(s.ok() && response.code() < 0) {
        errcode = response.code();
        errmsg.assign(response.error_msg());
    }
}

void grpcClient::rpcVerify(const std::string& token, int32_t& userid, std::string& username, int& errcode) {
    ClientContext ctx;
    auth::VerifyTokenRequest request;
    auth::VerifyTokenResponse response;

    request.set_token(token);
    Status s = this->Authstub->Verify(&ctx, request, &response);

    if(s.ok()) {
        if(response.code() < 0) {
            errcode = response.code();

            return;
        }

        userid = response.userid();
        username.assign(response.username());
    }
}

std::string grpcClient::rpcinitialPullMessage(const int32_t& userid, const std::string& username, const int messagecount) {
    ClientContext ctx;
    logic::pullMessageRequest request;
    logic::pullMessageResponse response;

    request.set_userid(userid);
    request.set_username(username);
    request.set_messagecount(messagecount);

    Status s = this->Logicstub->initialPullMessage(&ctx, request, &response);
    std::string helloMessage;

    if(s.ok()) helloMessage = response.message();

    return helloMessage;
}

std::string grpcClient::rpcCilentMessage(const std::string& message, int32_t& userid, const std::string& username) {
    ClientContext ctx;
    logic::clientMessageRequest request;
    logic::clientMessageResponse response;

    request.set_message(message);
    request.set_userid(userid);
    request.set_username(username);

    Status s = this->Logicstub->clientMessage(&ctx, request, &response);

    if(s.ok() && !response.message().empty()) return response.message();

    return "";
}

void grpcClient::rpcclearCursors(const int32_t& userid) {
    ClientContext ctx;
    logic::clearCursorsRequest request;
    logic::clearCursorsResponse response;

    request.set_userid(userid);

    Status s = this->Logicstub->clearCursors(&ctx, request, &response);

    if(s.ok()) return;

    return;
}

std::vector<std::string> grpcClient::rpcGetUserRoomList(const int32_t& userid) {
    ClientContext ctx;
    room::GetUserRoomListRequest request;
    room::GetUserRoomListResponse response;

    request.set_userid(userid);

    Status s = this->RoomStub->GetUserRoomList(&ctx, request, &response);
    std::vector<std::string> roomlist;

    if(s.ok()) {
        for(const auto& roominfo : response.roomlist()) {
            std::string roomid = roominfo.room_id();
            std::size_t colon_pos = roomid.find(":");
            if(colon_pos == std::string::npos) return {};

            std::string real_roomid = roomid.substr(0, colon_pos);
            roomlist.emplace_back(real_roomid);
        }
    }

    return roomlist;
}
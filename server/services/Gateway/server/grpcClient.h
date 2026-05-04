#pragma once

#include "auth.grpc.pb.h"
#include "auth.pb.h"
#include "logic.grpc.pb.h"
#include "logic.pb.h"
#include "room.grpc.pb.h"
#include "room.pb.h"

#include <memory>
#include <grpcpp/grpcpp.h>
#include <vector>
#include <functional>

using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::ClientReaderWriter;
using grpc::Status;

class HttpRequest;
class HttpResponse;

struct LogicInfo {
    int errcode;
    std::string errmsg;
    std::string token;
    int32_t userid;
    std::string username;
};

struct RegisterInfo {
    int errcode;
    std::string errmsg;
};

class grpcClient {

public:
    enum class api_error_id {
        bad_request = -6,
        login_failed_password_error,
        login_failed_email_error,
        email_exists,
        username_exists,
        Unknown_error
    };

    grpcClient() : Authchannel(grpc::CreateChannel("127.0.0.1:5006", grpc::InsecureChannelCredentials())), 
    Authstub(auth::AuthServer::NewStub(this->Authchannel)), 
    Logicchannel(grpc::CreateChannel("127.0.0.1:5008", grpc::InsecureChannelCredentials())), 
    Logicstub(logic::LogicServer::NewStub(this->Logicchannel)),
    Roomchannel(grpc::CreateChannel("127.0.0.1:5007", grpc::InsecureChannelCredentials())),
    RoomStub(room::RoomServer::NewStub(this->Roomchannel)) {}
    ~grpcClient() = default;

    // void rpcLoginAsync(const HttpRequest&, int& errcode, std::string& errmsg, std::function<void(LogicInfo)>);
    // void rpcRegisterAsync(const HttpRequest&, int& errcode, std::string& errmsg, std::function<void(RegisterInfo)>);
    void rpcCilentMessageAsync(const std::string& message, int32_t userid, std::string username, 
    std::function<void(std::string)> callback);
    void rpcclearCursorsAsync(int32_t userid, std::function<void()>);
    void rpcGetUserRoomListAsync(int32_t userid, const std::string& addr, const std::function<void(std::vector<std::string>&)>&);
    // void rpcJoinSessionAsync(int32_t userid, const std::string& roomname, const std::function<void(int, const std::string&, int64_t)>&);
    // void rpcCreateSessionAsync(int32_t userid, const std::string& roomname, const std::function<void(int32_t, const std::string&, int64_t)>&);
    void rpcJoinRooms(int32_t userid, std::vector<std::string>& rooms);
    void rpcBathPullMessageAsync(const std::string& message, std::function<void(const std::string&)> callback);
    void rpcIsSubSessionAsync(int32_t userid, std::string& room_id, const std::function<void(const std::string&)>& callback);
    void rpcPullMessageAsync(int64_t roomid, std::string& roomname, const std::function<void(const std::string&)>& callback);

    static std::string api_error_id_to_string(api_error_id id);
    static std::optional<api_error_id> to_api_error_id(int v);


private:
    std::shared_ptr<grpc::Channel> Authchannel;
    std::shared_ptr<grpc::Channel> Logicchannel;
    std::shared_ptr<grpc::Channel> Roomchannel;
    std::unique_ptr<auth::AuthServer::Stub> Authstub;
    std::unique_ptr<logic::LogicServer::Stub> Logicstub;
    std::unique_ptr<room::RoomServer::Stub> RoomStub;
};

using grpcClientPtr = std::shared_ptr<grpcClient>;

class BathPullClientReactor : public ::grpc::ClientReadReactor<logic::bathPullMessageResponse> {

public:
    BathPullClientReactor(logic::LogicServer::Stub*, const std::string& msg, 
        const std::function<void(const std::string&)>&, const std::function<void(const ::grpc::Status&)>&);

    void OnReadDone(bool ok) override;
    void OnDone(const ::grpc::Status& status) override;

private:
    ::grpc::ClientContext context_;
    logic::bathPullMessageRequest request_;
    logic::bathPullMessageResponse response_;

    std::function<void(const std::string&)> on_message_cb_;
    std::function<void(const ::grpc::Status&)> on_done_cb_;

};

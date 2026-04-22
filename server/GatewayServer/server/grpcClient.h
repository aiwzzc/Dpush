#pragma once

#include "../proto/auth.grpc.pb.h"
#include "../proto/auth.pb.h"
#include "../proto/logic.grpc.pb.h"
#include "../proto/logic.pb.h"
#include "../proto/room.grpc.pb.h"
#include "../proto/room.pb.h"

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

    void rpcLoginAsync(const HttpRequest&, int& errcode, std::string& errmsg, std::function<void(LogicInfo)>);
    void rpcRegisterAsync(const HttpRequest&, int& errcode, std::string& errmsg, std::function<void(RegisterInfo)>);
    void rpcinitialPullMessageAsync(int32_t userid, std::string username, const int messagecount,
    std::function<void(std::string)> callback);
    void rpcCilentMessageAsync(const std::string& message, int32_t userid, std::string username, 
    std::function<void(std::string)> callback);
    void rpcclearCursorsAsync(int32_t userid, std::function<void()>);
    void rpcGetUserRoomListAsync(int32_t userid, std::function<void(std::vector<std::string>)>);
    void rpcJoinRoomAsync(int32_t userid, const std::string& room_id, const std::function<void(int)>&);
    void rpcJoinRooms(int32_t userid, std::vector<std::string>& rooms);
    void rpcBathPullMessageAsync(const std::string& message, std::function<void(const std::string&)> callback);

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
    BathPullClientReactor(logic::LogicServer::Stub*, ::grpc::ClientContext*, logic::bathPullMessageRequest*,
        const std::function<void(const std::string&)>&, const std::function<void(const ::grpc::Status&)>&);

    void OnReadDone(bool ok) override;
    void OnDone(const ::grpc::Status& status) override;

private:
    ::grpc::ClientContext* context_;
    logic::bathPullMessageResponse response_;

    std::function<void(const std::string&)> on_message_cb_;
    std::function<void(const ::grpc::Status&)> on_done_cb_;

};

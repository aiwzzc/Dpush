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

using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::ClientReaderWriter;
using grpc::Status;

class HttpRequest;
class HttpResponse;

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

    void rpcLogin(const HttpRequest&, HttpResponse&, int& errcode, std::string& errmsg);
    void rpcRegister(const HttpRequest&, HttpResponse&, int& errcode, std::string& errmsg);
    void rpcVerify(const std::string&, int32_t&, std::string& username, int&);
    std::string rpcinitialPullMessage(const int32_t& userid, const std::string& username, const int messagecount);
    std::string rpcCilentMessage(const std::string& message, int32_t& userid, const std::string& username);
    void rpcclearCursors(const int32_t& userid);
    std::vector<std::string> rpcGetUserRoomList(const int32_t& userid);

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

#pragma once

#include "logic.grpc.pb.h"
#include "logic.pb.h"
#include "room.grpc.pb.h"
#include "room.pb.h"

#include "etcdServiceNode/grpcClientPool.hpp"

#include <memory>
#include <grpcpp/grpcpp.h>
#include <vector>
#include <functional>

using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::ClientReaderWriter;
using grpc::Status;

class grpcClient {

public:

    grpcClient(pulse::net::ServiceRegistryClient* ServiceRegistryClient) : 
    ServiceRegistryClient_(ServiceRegistryClient) {}
    ~grpcClient() = default;

    void start();

    void rpcCilentMessageAsync(const std::string& message, int32_t userid, std::string username, 
    std::function<void(std::string)> callback);
    void rpcclearCursorsAsync(int32_t userid, std::function<void()>);
    void rpcGetUserRoomListAsync(int32_t userid, const std::string& addr, const std::function<void(std::vector<std::string>&)>&);
    void rpcJoinRooms(int32_t userid, std::vector<std::string>& rooms);
    void rpcBathPullMessageAsync(const std::string& message, std::function<void(const std::string&)> callback);
    void rpcIsSubSessionAsync(int32_t userid, std::string& room_id, const std::function<void(const std::string&)>& callback);
    void rpcPullMessageAsync(int64_t roomid, std::string& roomname, const std::function<void(const std::string&)>& callback);

private:
    std::string logic_prefix_{"/services/logic/"};
    std::string room_prefix_{"/services/room/"};

    pulse::net::ServiceRegistryClient* ServiceRegistryClient_;
    pulse::net::RpcClientPool<logic::LogicServer> logic_stubs_;
    pulse::net::RpcClientPool<room::RoomServer> room_stubs_;
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

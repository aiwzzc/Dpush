#pragma once

#include "room.grpc.pb.h"
#include "room.pb.h"

#include <grpcpp/grpcpp.h>
#include <sw/redis++/redis++.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerReaderWriter;
using grpc::Status;

class RoomServer final : public room::RoomServer::Service {

public:
    RoomServer(sw::redis::Redis*);
    ~RoomServer();

    Status GetUserRoomList(::grpc::ServerContext* context, const ::room::GetUserRoomListRequest* request, 
        ::room::GetUserRoomListResponse* response) override;

    Status JoinRoom(::grpc::ServerContext* context, const ::room::JoinRoomRequest* request, 
        ::room::JoinRoomResponse* response) override;

    Status JoinRooms(::grpc::ServerContext* context, ::grpc::ServerReader<::room::JoinRoomRequest>* reader, 
        ::room::JoinRoomResponse* response) override;

    Status IsSubRoom(::grpc::ServerContext* context, const ::room::IsSubRoomRequest* request,
        ::room::IsSubRoomResponse* response) override;

private:
    sw::redis::Redis* redis_pool_;
};
#include "roomServer.h"
#include <unordered_set>
#include <iostream>
RoomServer::RoomServer(sw::redis::Redis* redis) : redis_pool_(redis) {}
RoomServer::~RoomServer() = default;

Status RoomServer::GetUserRoomList(::grpc::ServerContext* context, const ::room::GetUserRoomListRequest* request, 
    ::room::GetUserRoomListResponse* response) {

    std::unordered_set<std::string> roomlists;
    std::string userid = std::to_string(request->userid());
    this->redis_pool_->smembers(userid, std::inserter(roomlists, roomlists.begin()));

    for(const auto& room_id : roomlists) {
        auto* info = response->add_roomlist();
        info->set_room_id(room_id);
    }

    return Status::OK;
}

Status RoomServer::JoinRoom(::grpc::ServerContext* context, const ::room::JoinRoomRequest* request, 
    ::room::JoinRoomResponse* response) {

    std::string userid = std::to_string(request->userid());
    bool exists = this->redis_pool_->sismember(userid, request->room_id());

    if(!exists) {
        this->redis_pool_->sadd(userid, request->room_id());
        this->redis_pool_->sadd(request->room_id(), userid);
    }

    return Status::OK;
}
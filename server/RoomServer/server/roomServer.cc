#include "roomServer.h"
#include "../../flatbuffers/chat_generated.h"

#include <unordered_set>
#include <iostream>

RoomServer::RoomServer(sw::redis::Redis* redis) : redis_pool_(redis) {}
RoomServer::~RoomServer() = default;

Status RoomServer::GetUserRoomList(::grpc::ServerContext* context, const ::room::GetUserRoomListRequest* request, 
    ::room::GetUserRoomListResponse* response) {

    std::unordered_set<std::string> roomlists;
    std::string user_rooms_key = "{user:rooms:" + std::to_string(request->userid()) + "}";
    this->redis_pool_->smembers(user_rooms_key, std::inserter(roomlists, roomlists.begin()));

    for(const auto& room_id : roomlists) {
        auto* info = response->add_roomlist();
        info->set_room_id(room_id);
    }

    return Status::OK;
}

Status RoomServer::JoinRoom(::grpc::ServerContext* context, const ::room::JoinRoomRequest* request, 
    ::room::JoinRoomResponse* response) {

    std::string userid = std::to_string(request->userid());

    this->redis_pool_->sadd(userid, request->room_id());
    this->redis_pool_->sadd(request->room_id(), userid);

    response->set_code(0);
    response->set_error_msg("ok");

    return Status::OK;
}

Status RoomServer::JoinRooms(::grpc::ServerContext* context, ::grpc::ServerReader<::room::JoinRoomRequest>* reader, 
    ::room::JoinRoomResponse* response) {

    ::room::JoinRoomRequest req;

    while(reader->Read(&req)) {
        std::string userid = std::to_string(req.userid());

        this->redis_pool_->sadd(userid, req.room_id());
        this->redis_pool_->sadd(req.room_id(), userid);
    }

    response->set_code(0);
    response->set_error_msg("ok");

    return Status::OK;
}

Status RoomServer::IsSubRoom(::grpc::ServerContext* context, const ::room::IsSubRoomRequest* request,
    ::room::IsSubRoomResponse* response) {

    int32_t userid = request->userid();
    std::string room_id = request->room_id();
    std::string user_rooms_key = "{user:rooms:" + std::to_string(userid) + "}";

    bool exist = this->redis_pool_->sismember(user_rooms_key, room_id);
    
    thread_local flatbuffers::FlatBufferBuilder builder(4096);
    builder.Clear();

    auto action_offset = builder.CreateString("subscribe_ack");
    auto room_id_offset = builder.CreateString(room_id);
    auto status_offset = builder.CreateString(exist ? "ok" : "false");

    ChatApp::SignalingFromServerPayloadBuilder sigServerBuilder(builder);
    sigServerBuilder.add_action(action_offset);
    sigServerBuilder.add_room_id(room_id_offset);
    sigServerBuilder.add_status(status_offset);
    auto sigServerOffset = sigServerBuilder.Finish();

    ChatApp::RootMessageBuilder rootBuilder(builder);
    rootBuilder.add_payload_type(ChatApp::AnyPayload_SignalingFromServerPayload);
    rootBuilder.add_payload(sigServerOffset.Union());
    auto rootMsgOffset = rootBuilder.Finish();

    builder.Finish(rootMsgOffset);
    
    uint8_t* data = builder.GetBufferPointer();
    std::size_t size = builder.GetSize();

    response->set_message(std::string(reinterpret_cast<const char*>(data), size));

    return Status::OK;
}
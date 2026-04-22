#include "LogicGrpcServer.h"

#include "../../proto/room.grpc.pb.h"
#include "../../proto/room.pb.h"
#include "../base/typeCommon.h"
#include "../../flatbuffers/chat_generated.h"

#include <memory>

namespace {

template <typename... Args>
std::string FormatString(const std::string &format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

};

LogicGrpcServer::LogicGrpcServer(MySQLConnPool* mysql, sw::redis::Redis* redis, ComputeThreadPool* thread) : 
mysql_pool_(mysql), redis_pool_(redis), thread_pool_(thread) {}
LogicGrpcServer::~LogicGrpcServer() = default;

struct QueryAwaiter {

    MySQLConnPool* pool_;
    std::string sql_;
    SQLOperation::SQLType type_;
    SQLResult result_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->pool_->query(this->sql_, this->type_, [this, handle] (SQLResult res) {
            if(type_ == SQLOperation::SQLType::QUERY) this->result_ = res;

            handle.resume();
        });
    }

    SQLResult await_resume() { return this->result_; }
};

struct UpdateAwaiter {

    MySQLConnPool* pool_;
    std::string sql_;
    SQLOperation::SQLType type_;
    std::string status_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->pool_->query(this->sql_, this->type_, [this, handle] (SQLResult res) {
            if(!res.empty() && !res[0].empty()) this->status_.assign(res[0][0]);

            handle.resume();
        });
    }

    std::string await_resume() { return this->status_; }
};

static uint64_t getCurrentTimestamp() {
    auto now = std::chrono::system_clock::time_point::clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return milliseconds.count(); //单位是毫秒
}

struct clearCursorsAwaiter {

    sw::redis::Redis* redis_;
    ComputeThreadPool* threadpool_;
    int32_t userid_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->threadpool_->submit([this, handle] () {
            this->redis_->del(std::to_string(this->userid_));

            handle.resume();
        });
    }

    bool await_resume() { return true; }
};

struct GetRoomListAwaiter {

    int32_t userid_;
    std::unordered_map<std::string, std::string>* roomlist_;
    ComputeThreadPool* threadpool_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->threadpool_->submit([this, handle] () {
            auto channel = grpc::CreateChannel("127.0.0.1:5007", grpc::InsecureChannelCredentials());
            std::unique_ptr<room::RoomServer::Stub> stub = room::RoomServer::NewStub(channel);

            grpc::ClientContext ctx;
            room::GetUserRoomListResponse res;
            room::GetUserRoomListRequest req;
            req.set_userid(this->userid_);

            Status s = stub->GetUserRoomList(&ctx, req, &res);

            if(s.ok()) {
                for(const auto& roominfo : res.roomlist()) {
                    std::string roomid = roominfo.room_id();
                    std::size_t colon_pos = roomid.find(":");
                    if(colon_pos == std::string::npos) continue;

                    std::string real_roomid = roomid.substr(0, colon_pos);
                    std::string real_roomname = roomid.substr(colon_pos + 1);

                    this->roomlist_->insert({real_roomid, real_roomname});
                }
            }

            handle.resume();
        });
    }

    bool await_resume() { return true; }
};

struct GetUserCursors {

    ComputeThreadPool* threadpool_;
    sw::redis::Redis* redis_;
    std::string userid_;
    std::unordered_map<std::string, std::string> cursors_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->threadpool_->submit([this, handle] () {
            std::unordered_map<std::string, std::string> cursors;
            this->redis_->hgetall(this->userid_, std::inserter(cursors, cursors.begin()));

            this->cursors_ = std::move(cursors);
            handle.resume();
        });
    }

    std::unordered_map<std::string, std::string> await_resume() { return this->cursors_; }

};

struct GetHistoryMessageAwaiter {

    using streamMsg = std::pair<std::string, std::unordered_map<std::string, std::string>>;

    ComputeThreadPool* threadpool_;
    sw::redis::Redis* redis_;
    std::string userid_;
    std::string roomid_;
    std::unordered_map<std::string, std::string> cursors_;
    int messagecount_;
    MessageBatch messages_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->threadpool_->submit([this, handle] () {
            std::string last_message_id = this->cursors_.empty() ? "" : this->cursors_[this->roomid_];
            int get_count = messagecount_;
            
            std::string stream_ref = last_message_id.empty() ? "+" : "(" + last_message_id;
    
            std::vector<streamMsg> messages;
            std::string messageKey = "{" + this->roomid_ + "}";

            this->redis_->xrevrange(messageKey, stream_ref, "-", get_count, std::back_inserter(messages));
            if(!messages.empty()) this->redis_->hset(this->userid_, this->roomid_, messages.back().first);
    
            for(const auto& msg : messages) {

                for(auto it = msg.second.begin(); it != msg.second.end(); ++it) {
                    
                    auto rootMsg = ChatApp::GetRootMessage(it->second.data());
                    auto payload = rootMsg->payload_as_ServerMessagePayload();
                    for(const auto& serverMsg : *payload->messages()) {
                        Message message;
                        message.id = msg.first;

                        message.content = serverMsg->content()->c_str();
                        message.timestamp = serverMsg->timestamp();
                        message.user_id = serverMsg->user()->userid();
                        message.username = serverMsg->user()->username()->c_str();

                        this->messages_.messages.emplace_back(std::move(message));
                    }
                }
            }

            if(messages.size() < messagecount_) this->messages_.has_more = false;
            else this->messages_.has_more = true;

            handle.resume();
        });
    }

    MessageBatch await_resume() { return this->messages_; }

};

struct PullMessageAwaiter {

    using streamMsg = std::pair<std::string, std::unordered_map<std::string, std::string>>;

    sw::redis::Redis* redis_;
    ComputeThreadPool* thread_pool_;
    std::string_view room_id_;
    int64_t message_id_;
    std::vector<streamMsg> messages_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->thread_pool_->submit([this, handle] () {
            std::string messageKey = "{" + std::string(this->room_id_.data(), this->room_id_.size()) + "}";
            std::string stream_ref;
            int get_count = 50;

            if(message_id_) {
                stream_ref = "(" + std::to_string(message_id_);
                this->redis_->xrange(messageKey, stream_ref, "+", get_count, std::back_inserter(this->messages_));

            } else {
                stream_ref = "+";
                this->redis_->xrevrange(messageKey, stream_ref, "-", get_count, std::back_inserter(this->messages_));
            }

            handle.resume();
        });
    }

    std::vector<streamMsg> await_resume() { return this->messages_; }
};

QueryAwaiter async_query_for_coro(MySQLConnPool* pool, const std::string& sql, SQLOperation::SQLType type) {
    return QueryAwaiter{pool, sql, type, {}};
}

UpdateAwaiter async_update_for_coro(MySQLConnPool* pool, const std::string& sql, SQLOperation::SQLType type) {
    return UpdateAwaiter{pool, sql, type, ""};
}

clearCursorsAwaiter async_clearCursors_for_coro(sw::redis::Redis* redis, ComputeThreadPool* threadpool, int32_t& userid) {
    return clearCursorsAwaiter{redis, threadpool, userid};
}

GetRoomListAwaiter async_getRoomList_for_coro(const int32_t& userid, std::unordered_map<std::string, std::string>* roomlist, 
ComputeThreadPool* threadpool) {
    return GetRoomListAwaiter{userid, roomlist, threadpool};
}

GetUserCursors async_GetUserCursors_for_coro(ComputeThreadPool* threadpool, sw::redis::Redis* redis, const std::string& userid) {
    return GetUserCursors(threadpool, redis, userid, {});
}

GetHistoryMessageAwaiter async_GerHistoryMessage_for_coro(ComputeThreadPool* threadpool, sw::redis::Redis* redis, const std::string& userid, 
const std::string& roomid, std::unordered_map<std::string, std::string>& cursors, int messagecount) {
    return GetHistoryMessageAwaiter{threadpool, redis, userid, roomid, cursors, messagecount, {}};
}

PullMessageAwaiter async_PullMessage_for_coro(sw::redis::Redis* redis, ComputeThreadPool* threadpool, std::string_view room_id, int64_t& msgid) {
    return PullMessageAwaiter{redis, threadpool, room_id, msgid, {}};
}

DetachedTask LogicGrpcServer::DoinitialPullMessage(grpc::ServerUnaryReactor* reatcor, const logic::pullMessageRequest* request, 
    logic::pullMessageResponse* response) {

    std::string_view userid = std::to_string(request->userid());

    std::unordered_map<std::string, std::string> roomlist;

    bool ok = co_await async_getRoomList_for_coro(request->userid(), &roomlist, this->thread_pool_);

    if (roomlist.empty()) {
        response->set_message("");

        reatcor->Finish(grpc::Status::OK);
        co_return;
    }

    std::unordered_map<std::string, std::string> cursors = co_await async_GetUserCursors_for_coro(this->thread_pool_,
    this->redis_pool_, userid.data());

    std::vector<RoomDataCache> all_room_data;

    for(auto it = roomlist.begin(); it != roomlist.end(); ++it) {
        MessageBatch msgs = co_await async_GerHistoryMessage_for_coro(this->thread_pool_, this->redis_pool_,
        userid.data(), it->first, cursors, request->messagecount());

        all_room_data.emplace_back(it->first, it->second, msgs);
    }

    thread_local flatbuffers::FlatBufferBuilder builder(4096);
    builder.Clear();

    std::vector<flatbuffers::Offset<ChatApp::RoomItem>> roomItemOffsets;

    for(auto& room_data : all_room_data) {
        std::vector<flatbuffers::Offset<ChatApp::RoomMessageItem>> messageItem;

        for(auto& message : room_data.msgs.messages) {
            auto username = builder.CreateString(message.username);
            auto content_str = builder.CreateString(message.content);
            auto messageid_str = builder.CreateString(message.id);

            ChatApp::UserBuilder UserBuilder(builder);
            UserBuilder.add_userid(message.user_id);
            UserBuilder.add_username(username);
            auto Useroffset = UserBuilder.Finish();

            ChatApp::RoomMessageItemBuilder RoomMessageBuilder(builder);
            RoomMessageBuilder.add_user(Useroffset);
            RoomMessageBuilder.add_content(content_str);
            RoomMessageBuilder.add_id(messageid_str);
            RoomMessageBuilder.add_timestamp(message.timestamp);
            auto roomMsgOffset = RoomMessageBuilder.Finish();

            messageItem.push_back(roomMsgOffset);
        }

        auto messages_vector = builder.CreateVector(messageItem);
        auto roomid = builder.CreateString(room_data.room_id);
        auto roomname = builder.CreateString(room_data.room_name);

        ChatApp::RoomItemBuilder RoomItemBuilder(builder);
        RoomItemBuilder.add_room_id(roomid);
        RoomItemBuilder.add_roomname(roomname);
        RoomItemBuilder.add_has_more_messages(room_data.msgs.has_more);
        RoomItemBuilder.add_messages(messages_vector);
        auto roomItemOffset = RoomItemBuilder.Finish();

        roomItemOffsets.push_back(roomItemOffset);
    }

    auto rooms_vector = builder.CreateVector(roomItemOffsets);
    auto my_username_str = builder.CreateString(request->username());

    ChatApp::UserBuilder meBuilder(builder);
    meBuilder.add_userid(request->userid());
    meBuilder.add_username(my_username_str);

    auto meOffset = meBuilder.Finish();

    ChatApp::HelloMessagePayloadBuilder helloPayloadBuilder(builder);
    helloPayloadBuilder.add_me(meOffset);
    helloPayloadBuilder.add_rooms(rooms_vector);

    auto helloPayloadOffset = helloPayloadBuilder.Finish();

    ChatApp::RootMessageBuilder rootBuilder(builder);
    rootBuilder.add_payload_type(ChatApp::AnyPayload_HelloMessagePayload);
    rootBuilder.add_payload(helloPayloadOffset.Union());
    auto rootMsgOffset = rootBuilder.Finish();

    builder.Finish(rootMsgOffset);

    uint8_t* data = builder.GetBufferPointer();
    int size = builder.GetSize();

    response->set_message(std::string(reinterpret_cast<const char*>(data), size));

    reatcor->Finish(grpc::Status::OK);
}

grpc::ServerUnaryReactor* LogicGrpcServer::initialPullMessage(grpc::CallbackServerContext* context, const logic::pullMessageRequest* request, 
    logic::pullMessageResponse* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    DoinitialPullMessage(reactor, request, response);

    return reactor;
}

grpc::ServerUnaryReactor* LogicGrpcServer::clientMessage(grpc::CallbackServerContext* context, const logic::clientMessageRequest* request, 
    logic::clientMessageResponse* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    DoclientMessage(reactor, request, response);

    return reactor;
}

DetachedTask LogicGrpcServer::DoclientMessage(grpc::ServerUnaryReactor* reactor, const logic::clientMessageRequest* request, 
    logic::clientMessageResponse* response) {

    auto rootMsg = ChatApp::GetRootMessage(request->message().data());

    switch(rootMsg->payload_type()) {
        case ChatApp::AnyPayload_PullMissingMessagePayload: {
            auto payload = rootMsg->payload_as_PullMissingMessagePayload();

            int32_t userid = request->userid();
            std::string useridStr = std::to_string(userid);
            std::string username{request->username()};

            std::string_view room_id(payload->room_id()->c_str(), payload->room_id()->size());
            auto miss_ids = payload->missing_message_ids();

            co_return;
        }

        case ChatApp::AnyPayload_RequestRoomHistoryPayload: {
            auto payload = rootMsg->payload_as_RequestRoomHistoryPayload();

            int32_t userid = request->userid();
            std::string useridStr = std::to_string(userid);
            std::string username{request->username()};
            std::string_view room_id{payload->room_id()->c_str(), payload->room_id()->size()};
            std::string_view first_message_id{payload->first_message_id()->c_str(), payload->first_message_id()->size()};
            int PullMsgCount = payload->count();
            
            std::unordered_map<std::string, std::string> cursors = co_await async_GetUserCursors_for_coro(this->thread_pool_,
            this->redis_pool_, useridStr);

            MessageBatch msgs = co_await async_GerHistoryMessage_for_coro(this->thread_pool_, this->redis_pool_,
            useridStr, room_id.data(), cursors, 10);

            thread_local flatbuffers::FlatBufferBuilder builder(4096);
            builder.Clear();

            std::vector<flatbuffers::Offset<ChatApp::RequestMessageItem>> requestMsgItemOffsets;

            for(const auto& msg : msgs.messages) {
                auto username_str = builder.CreateString(msg.username);
                auto msg_id_str = builder.CreateString(msg.id);
                auto content_str = builder.CreateString(msg.content);

                ChatApp::UserBuilder Userbuilder(builder);
                Userbuilder.add_userid(msg.user_id);
                Userbuilder.add_username(username_str);
                auto UserOffset = Userbuilder.Finish();

                ChatApp::RequestMessageItemBuilder requestMsgBuilder(builder);
                requestMsgBuilder.add_user(UserOffset);
                requestMsgBuilder.add_content(content_str);
                requestMsgBuilder.add_id(msg_id_str);
                requestMsgBuilder.add_timestamp(msg.timestamp);
                auto requestMsgOffset = requestMsgBuilder.Finish();

                requestMsgItemOffsets.push_back(requestMsgOffset);
            }

            auto requestMsgItemOffset = builder.CreateVector(requestMsgItemOffsets);
            auto roomid_str = builder.CreateString(room_id.data());
            auto roomname_str = builder.CreateString(username.data());

            ChatApp::RequestMessagePayloadBuilder requestMsgPayloadBuilder(builder);
            requestMsgPayloadBuilder.add_messages(requestMsgItemOffset);
            requestMsgPayloadBuilder.add_has_more_messages(msgs.has_more);
            requestMsgPayloadBuilder.add_room_id(roomid_str);
            requestMsgPayloadBuilder.add_roomname(roomname_str);
            auto requestMsgPayloadOffset = requestMsgPayloadBuilder.Finish();

            ChatApp::RootMessageBuilder rootMsgBuilder(builder);
            rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_RequestMessagePayload);
            rootMsgBuilder.add_payload(requestMsgPayloadOffset.Union());
            auto rootMsgOffset = rootMsgBuilder.Finish();

            builder.Finish(rootMsgOffset);

            uint8_t* data = builder.GetBufferPointer();
            int size = builder.GetSize();

            response->set_ok(true);
            response->set_message(std::string(reinterpret_cast<const char*>(data), size));

            reactor->Finish(grpc::Status::OK);
            co_return;
        }
    }

    reactor->Finish(grpc::Status::OK);
}

grpc::ServerUnaryReactor* LogicGrpcServer::clearCursors(grpc::CallbackServerContext* context, const logic::clearCursorsRequest* request, 
    logic::clearCursorsResponse* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    DoclearCursors(reactor, request, response);

    return reactor;
}

DetachedTask LogicGrpcServer::DoclearCursors(grpc::ServerUnaryReactor* reactor, const logic::clearCursorsRequest* request,
    logic::clearCursorsResponse* response) {

    int32_t userid = request->userid();
    bool ok = co_await async_clearCursors_for_coro(this->redis_pool_, this->thread_pool_, userid);

    reactor->Finish(grpc::Status::OK);
}

BatchPullReactor::BatchPullReactor(const logic::bathPullMessageRequest* req, sw::redis::Redis* redis, ComputeThreadPool* threadpool) :
request_(req), redis_pool_(redis), thread_pool_(threadpool) {
    this->FetchNextAndWrite();
}

void BatchPullReactor::OnWriteDone(bool ok) {
    if(!ok) {
        Finish(::grpc::Status::CANCELLED);
        return;
    }

    this->FetchNextAndWrite();
}

void BatchPullReactor::OnDone() {
    delete this;
}

DetachedTask BatchPullReactor::FetchNextAndWrite() {
    auto rootMsg = ChatApp::GetRootMessage(this->request_->message().data());

    auto payload = rootMsg->payload_as_BatchPullMessagePayload();
    auto rooms = payload->rooms();

    if(this->current_index_ >= rooms->size()) {
        Finish(::grpc::Status::OK);
        co_return;
    }

    auto item = rooms->Get(this->current_index_++);

    std::string_view room_id(item->room_id()->c_str(), item->room_id()->size());
    int64_t last_message_id = item->last_message_id();

    auto messages = co_await async_PullMessage_for_coro(this->redis_pool_, this->thread_pool_, room_id, last_message_id);
    
    std::string batch_msg;
    for(auto& message : messages) {
        for(auto it = message.second.begin(); it != message.second.end(); ++it) {
            batch_msg.append(it->second);
        }
    }

    this->response_.set_message(batch_msg);

    StartWrite(&this->response_);
}

grpc::ServerWriteReactor<logic::bathPullMessageResponse>* LogicGrpcServer::bathPullMessage(grpc::CallbackServerContext* context,
    const logic::bathPullMessageRequest* request) {

    return new BatchPullReactor(request, this->redis_pool_, this->thread_pool_);
}

grpc::ServerUnaryReactor* LogicGrpcServer::joinSession(grpc::CallbackServerContext* context, const logic::joinSessionRequest* request, 
    logic::joinSessionResponse* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    DojoinSession(reactor, request, response);

    return reactor;
}

DetachedTask LogicGrpcServer::DojoinSession(grpc::ServerUnaryReactor* reactor, const logic::joinSessionRequest* request, 
    logic::joinSessionResponse* response) {

    int userid = request->userid();
    std::string roomname = request->roomname();

    // mysql get room_id
    std::string sql = FormatString("select room_id from room_info where room_name = '%s'", roomname.c_str());
    SQLResult result = co_await async_query_for_coro(this->mysql_pool_, sql, SQLOperation::SQLType::QUERY);

    if(result.empty()) {
        response->set_message("");

        reactor->Finish(grpc::Status::OK);
        co_return;
    }

    // get flatbuffer to Gateway and redis publish
    std::string room_id = result[0][0];
    int64_t msg_id = 0;

    auto messages = co_await async_PullMessage_for_coro(this->redis_pool_, this->thread_pool_, room_id, msg_id);

    thread_local flatbuffers::FlatBufferBuilder builder(4096);
    builder.Clear();

    std::vector<flatbuffers::Offset<ChatApp::ServerMessageItem>> new_item_offsets;

    for(auto& message : messages) {
        for(auto it = message.second.begin(); it != message.second.end(); ++it) {
            auto rootMsg = ChatApp::GetRootMessage(it->second.data());
            auto payload = rootMsg->payload_as_ServerMessagePayload();
            auto old_messages = payload->messages();

            for(int i = 0; i < old_messages->size(); ++i) {
                auto old_item = old_messages->Get(i);

                auto user = old_item->user();
                auto username = builder.CreateString(user->username()->c_str());

                ChatApp::UserBuilder userBuilder(builder);
                userBuilder.add_userid(user->userid());
                userBuilder.add_username(username);
                auto new_user_offset = userBuilder.Finish();

                auto client_message_id = builder.CreateString(old_item->client_message_id()->c_str());
                auto content = builder.CreateString(old_item->content()->c_str());

                ChatApp::ServerMessageItemBuilder serverMessageItemBuilder(builder);
                serverMessageItemBuilder.add_client_message_id(client_message_id);
                serverMessageItemBuilder.add_content(content);
                serverMessageItemBuilder.add_server_message_id(old_item->server_message_id());
                serverMessageItemBuilder.add_timestamp(old_item->timestamp());
                serverMessageItemBuilder.add_user(new_user_offset);

                new_item_offsets.push_back(serverMessageItemBuilder.Finish());
            }
        }
    }

    auto new_room_id = builder.CreateString(room_id);
    auto new_roomname = builder.CreateString(roomname);
    auto new_messages_vector = builder.CreateVector(new_item_offsets);

    ChatApp::JoinSessionPayloadBuilder joinBuilder(builder);
    joinBuilder.add_room_id(new_room_id);
    joinBuilder.add_roomname(new_roomname);
    joinBuilder.add_messages(new_messages_vector);
    auto joinPayloadOffset = joinBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_JoinSessionPayload);
    rootMsgBuilder.add_payload(joinPayloadOffset.Union());
    auto rootMsgOffset = rootMsgBuilder.Finish();

    builder.Finish(rootMsgOffset);
    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    std::size_t size = builder.GetSize();

    response->set_message(std::string(data, size));

    this->redis_pool_->publish("room1:" + room_id, std::to_string(userid));

    reactor->Finish(grpc::Status::OK);
}
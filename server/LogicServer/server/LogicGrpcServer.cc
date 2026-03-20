#include "LogicGrpcServer.h"

#include "../../proto/room.grpc.pb.h"
#include "../../proto/room.pb.h"
#include "../base/typeCommon.h"

#include <memory>
#include <jsoncpp/json/json.h>

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

static std::string SerializeMessageToJson(const Message& msg) {
    Json::Value root;
    root["content"] = msg.content;
    root["userid"] = msg.user_id;
    root["timestamp"] = msg.timestamp;
    root["username"] = msg.username;
    return root.toStyledString();
}

struct RedisXaddAwaiter {

    sw::redis::Redis* redis_;
    std::vector<Message> messages_;
    std::string roomid_;
    ComputeThreadPool* threadpool_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->threadpool_->submit([this, handle] () {
            for(auto& message : this->messages_) {
                std::string msg_json = SerializeMessageToJson(message);
                std::vector<std::pair<std::string, std::string>> fields{{"payload", msg_json}};

                message.id = this->redis_->xadd(this->roomid_, "*", fields.begin(), fields.end());
            }

            handle.resume();
        });
    }

    bool await_resume() { return true; }
};

static uint64_t getCurrentTimestamp() {
    auto now = std::chrono::system_clock::time_point::clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return milliseconds.count(); //单位是毫秒
}

struct ParseClientJsonAwaiter {

    Json::Value* root_;
    std::vector<Message>* messages_;
    int32_t userid_;
    std::string username_;
    ComputeThreadPool* threadpool_;
    std::string roomid_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->threadpool_->submit([this, handle] () {
            Json::Value root = *this->root_;
            if(root["payload"].isNull()) {
                // 日志
                handle.resume();
                return;
            }
            Json::Value payload = root["payload"];

            if(payload["roomId"].isNull()) {
                // 日志
                handle.resume();
                return;
            }
            std::string roomId = payload["roomId"].asString();

            if(payload["messages"].isNull()) {
                // 日志
                handle.resume();
                return;
            }
            Json::Value messages = payload["messages"];

            if(messages.isNull() || !messages.isArray()) {
                // 日志
                handle.resume();
                return;
            }

            this->roomid_ = roomId;

            for(int i = 0; i < messages.size(); ++i) {
                Json::Value msg = messages[i];
                Message message;
                if(msg["content"].isNull()) {
                    // 日志
                    continue;
                }
                message.content = msg["content"].asString();
                message.timestamp = getCurrentTimestamp();
                message.user_id = this->userid_;
                message.username = this->username_;
                this->messages_->push_back(message);
            }

            handle.resume();
        });
    }

    std::string await_resume() { return this->roomid_; }
};

static std::string buildWebSocketFrame(const std::string& payload, uint8_t opcode = 0x01) {
    std::string frame;

    frame.push_back(0x80 | (opcode & 0x0F));

    size_t payload_length = payload.size();
    if (payload_length <= 125) {
        frame.push_back(static_cast<uint8_t>(payload_length));
    } else if (payload_length <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payload_length & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<uint8_t>((payload_length >> (8 * i)) & 0xFF));
        }
    }

    frame += payload;

    return frame;
}

struct fillServerJsonAwaiter {
    
    ComputeThreadPool* threadpool_;
    std::vector<Message>* messages_;
    std::string roomid_;
    std::string serverMessage_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->threadpool_->submit([this, handle] () {
            Json::Value root;
            Json::Value payload;
            Json::Value messages;
            root["type"] = "ServerMessage";
            payload["roomId"] = this->roomid_;

            for(const auto& msg : *(this->messages_)) {
                Json::Value user;
                Json::Value Msg;

                Msg["id"] = msg.id;
                Msg["content"] = msg.content;
                user["userid"] = msg.user_id;
                user["username"] = msg.username;
                Msg["user"] = user;
                Msg["timestamp"] = msg.timestamp;
                messages.append(Msg);
            }

            if(!messages.empty()) payload["message"] = messages;
            else payload["message"] = Json::arrayValue;

            root["payload"] = payload;
            Json::FastWriter writer;
            this->serverMessage_ = buildWebSocketFrame(writer.write(root));

            handle.resume();
        });
    }

    std::string await_resume() { return this->serverMessage_; }
};

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

int fillMessageBatch(const std::pair<std::string, std::unordered_map<std::string, std::string>>& msgStream, MessageBatch& msgs) {
    const std::string& msg_id = msgStream.first;
    const std::unordered_map<std::string, std::string>& msg_fields = msgStream.second;

    for(auto it = msg_fields.begin(); it != msg_fields.end(); ++it) {
        Message msg;
        msg.id = msg_id;

        Json::Value root;
        Json::Reader reader;
       
        bool ok = reader.parse(it->second, root);
        if(!ok) return -1;

        if(root["content"].isNull()) {
            // 日志
            return -2;
        }
        msg.content = root["content"].asString();

        if(root["userid"].isNull()) {
            // 日志
            return -3;
        }
        msg.user_id = root["userid"].asInt64();

        if(root["timestamp"].isNull()) {
            // 日志
            return -4;
        }
        msg.timestamp = root["timestamp"].asUInt64();

        if(root["username"].isNull()) {
            // 日志
            return -4;
        }
        msg.username = root["username"].asString();

        msgs.messages.push_back(msg);
    }

    return 0;
}

struct GerHistoryMessageAwaiter {

    using streamMsg = std::pair<std::string, std::unordered_map<std::string, std::string>>;

    ComputeThreadPool* threadpool_;
    sw::redis::Redis* redis_;
    std::string userid_;
    std::string roomid_;
    std::unordered_map<std::string, std::string> cursors_;
    int messagecount_;
    // std::vector<streamMsg> messages_;
    MessageBatch messages_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->threadpool_->submit([this, handle] () {
            std::string last_message_id = this->cursors_.empty() ? "" : this->cursors_[this->roomid_];
            int get_count = messagecount_;
            
            std::string stream_ref = last_message_id.empty() ? "+" : "(" + last_message_id;
    
            std::vector<streamMsg> messages;

            this->redis_->xrevrange(this->roomid_, stream_ref, "-", get_count, std::back_inserter(messages));
            if(!messages.empty()) this->redis_->hset(this->userid_, this->roomid_, messages.back().first);
    
            for(const auto& msg : messages) {
                int ret = fillMessageBatch(msg, this->messages_);
                if(ret < 0) {
                    handle.resume();
                    return;
                }
            }

            if(messages.size() < messagecount_) this->messages_.has_more = false;
            else this->messages_.has_more = true;

            handle.resume();
        });
    }

    MessageBatch await_resume() { return this->messages_; }

};

struct MakeRequestMessageAwaiter {

    ComputeThreadPool* threadpool_;
    Json::Value* root_;
    std::string roomname_;
    MessageBatch msgs_;
    std::string websocketStr_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->threadpool_->submit([this, handle] () {
            Json::Value root = *this->root_;
            if(root["payload"].isNull()) {
                // 日志
                handle.resume();
                return;
            }
            Json::Value payload = root["payload"];

            if(payload["roomId"].isNull()) {
                // 日志
                handle.resume();
                return;
            }
            std::string roomId = payload["roomId"].asString();

            if(payload["firstMessageId"].isNull()) {
                // 日志
                handle.resume();
                return;
            }
            std::string firstMessageId = payload["firstMessageId"].asString();

            if(payload["count"].isNull()) {
                // 日志
                handle.resume();
                return;
            }
            int count = payload["count"].asInt();

            root = Json::Value{};
            payload = Json::Value{};
            Json::Value messages;
            root["type"] = "RequestMessage";
            payload["roomId"] = roomId;
            payload["roomname"] = roomname_;
            payload["hasMoreMessages"] = msgs_.has_more;

            for(const auto& message : msgs_.messages) {
                Json::Value user;
                Json::Value Msg;

                Msg["id"] = message.id;
                Msg["content"] = message.content;
                user["userid"] = message.user_id;
                user["username"] = message.username;
                Msg["user"] = user;
                Msg["timestamp"] = message.timestamp;
                messages.append(Msg);
            }

            if(!messages.empty()) payload["message"] = messages;
            else payload["message"] = Json::arrayValue;

            root["payload"] = payload;
            Json::FastWriter writer;
            std::string resJson = writer.write(root);

            this->websocketStr_ = buildWebSocketFrame(resJson);

            handle.resume();
        });
    }

    std::string await_resume() { return this->websocketStr_; }

};

QueryAwaiter async_query_for_coro(MySQLConnPool* pool, const std::string& sql, SQLOperation::SQLType type) {
    return QueryAwaiter{pool, sql, type, {}};
}

UpdateAwaiter async_update_for_coro(MySQLConnPool* pool, const std::string& sql, SQLOperation::SQLType type) {
    return UpdateAwaiter{pool, sql, type, ""};
}

RedisXaddAwaiter async_redisXadd_for_coro(sw::redis::Redis* redis, std::vector<Message>& messages, 
const std::string& roomid, ComputeThreadPool* threadpool){
    return RedisXaddAwaiter{redis, messages, roomid, threadpool};
}

ParseClientJsonAwaiter async_parseclientjson_for_coro(Json::Value* root, std::vector<Message>* messages, 
int32_t& userid, const std::string& username, ComputeThreadPool* threadpool) {
    return ParseClientJsonAwaiter{root, messages, userid, username, threadpool, ""};
}

fillServerJsonAwaiter async_fillserverjson_for_coro(ComputeThreadPool* threadpool, std::vector<Message>* messages, std::string& roomid) {
    return fillServerJsonAwaiter{threadpool, messages, roomid, ""};
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

GerHistoryMessageAwaiter async_GerHistoryMessage_for_coro(ComputeThreadPool* threadpool, sw::redis::Redis* redis, const std::string& userid, 
const std::string& roomid, std::unordered_map<std::string, std::string>& cursors, int messagecount) {
    return GerHistoryMessageAwaiter{threadpool, redis, userid, roomid, cursors, messagecount, {}};
}

MakeRequestMessageAwaiter async_MakeRequestMessage_for_coro(ComputeThreadPool* threadpool, Json::Value* root, const std::string& roomname,
const MessageBatch& messages) {
    return MakeRequestMessageAwaiter{threadpool, root, roomname, messages, ""};
}

void fillRoomJson(const MessageBatch& message_batch, Json::Value& room, const std::string& roomid, const std::string& roomname) {
    room["id"] = roomid;
    room["name"] = roomname;
    room["hasMoreMessage"] = message_batch.has_more;

    Json::Value messages;
    for(int j = 0; j < message_batch.messages.size(); j++) {
        Json::Value  message;
        Json::Value user;
        message["id"] = message_batch.messages[j].id;
        message["content"] = message_batch.messages[j].content;   
        user["id"] = message_batch.messages[j].user_id;
        user["username"] = message_batch.messages[j].username;
        message["user"] = user;
        message["timestamp"] = (Json::UInt64)message_batch.messages[j].timestamp;
        messages[j] = message;
    }

    if(message_batch.messages.size() > 0)
        room["messages"] = messages;
    else 
        room["messages"] = Json::arrayValue;  //不能为NULL，否则前端报异常
}

DetachedTask LogicGrpcServer::DoinitialPullMessage(grpc::ServerUnaryReactor* reatcor, const logic::pullMessageRequest* request, 
    logic::pullMessageResponse* response) {

    std::string userid = std::to_string(request->userid());

    Json::Value root;
    Json::Value payload;
    Json::Value me;

    root["type"] = "hello";

    me["id"] = userid;
    me["username"] = request->username();
    payload["me"] = me;

    Json::Value rooms;
    int it_index = 0;

    std::unordered_map<std::string, std::string> roomlist;

    bool ok = co_await async_getRoomList_for_coro(request->userid(), &roomlist, this->thread_pool_);

    if (roomlist.empty()) {
        reatcor->Finish(grpc::Status::OK);
        co_return;
    }

    std::unordered_map<std::string, std::string> cursors = co_await async_GetUserCursors_for_coro(this->thread_pool_,
    this->redis_pool_, userid);

    for(auto it = roomlist.begin(); it != roomlist.end(); ++it) {
        MessageBatch msgs = co_await async_GerHistoryMessage_for_coro(this->thread_pool_, this->redis_pool_,
        userid, it->first, cursors, request->messagecount());

        Json::Value room;
        fillRoomJson(msgs, room, it->first, it->second);
        rooms[it_index++] = room;
    }

    payload["rooms"] = rooms;
    root["payload"] = payload;
    Json::FastWriter writer;
    std::string str_json = writer.write(root);
    
    response->set_message(buildWebSocketFrame(str_json));

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

/*
{
  "type": "PullMissingMessages",
  "payload": {
    "roomId": "beast",
    "missingMessageIds": [11, 12]
  }
}
*/

DetachedTask LogicGrpcServer::DoclientMessage(grpc::ServerUnaryReactor* reactor, const logic::clientMessageRequest* request, 
    logic::clientMessageResponse* response) {

    Json::Value root;
    Json::Reader reader;

    if(!reader.parse(request->message(), root) || root["type"].isNull()) {
        reactor->Finish(grpc::Status::OK);
        co_return;
    }

    std::string type = root["type"].asString();

    if(type == "PullMissingMessages") {

        reactor->Finish(grpc::Status::OK);
        co_return;

    } else if(type == "RequestRoomHistory") {
        int32_t userid = request->userid();
        std::string useridStr = std::to_string(userid);
        std::string username = request->username();

        std::string roomid = root["payload"]["roomId"].asString();

        std::unordered_map<std::string, std::string> roomlist;

        bool ok = co_await async_getRoomList_for_coro(userid, &roomlist, this->thread_pool_);
        
        std::unordered_map<std::string, std::string> cursors = co_await async_GetUserCursors_for_coro(this->thread_pool_,
        this->redis_pool_, useridStr);

        MessageBatch msgs = co_await async_GerHistoryMessage_for_coro(this->thread_pool_, this->redis_pool_,
        useridStr, roomid, cursors, 10);            
        
        std::string RequestMessage = co_await async_MakeRequestMessage_for_coro(this->thread_pool_, &root, 
        roomlist[roomid], msgs);

        response->set_ok(true);
        response->set_message(RequestMessage);

        reactor->Finish(grpc::Status::OK);
        co_return;

    } else if(type == "clientCreateRoom") {

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
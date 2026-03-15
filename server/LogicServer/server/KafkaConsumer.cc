#include "KafkaConsumer.h"
#include <iostream>
#include <jsoncpp/json/json.h>

#include "../base/coroutineTask.h"
#include "../base/typeCommon.h"

KafkaConsumer::KafkaConsumer(const std::string& brokers, const std::string& group_id, 
    const std::vector<std::string>& topics, OrderedThreadPool* ordthreadpool, 
    ComputeThreadPool* comthreadpool, sw::redis::Redis* redis_pool) : 
    OrderedThreadPool_(ordthreadpool), ComputeThreadPool_(comthreadpool), redis_pool_(redis_pool), running_(true) {

    std::string errstr;
    RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    conf->set("bootstrap.servers", brokers, errstr);

    conf->set("group.id", group_id, errstr);

    conf->set("auto.offset.reset", "earliest", errstr);
        
    // 1. 开启后台秘书线程，每 5 秒帮我们去网络汇报一次（极大降低网络IO，提升吞吐）
    conf->set("enable.auto.commit", "true", errstr);

    // 2. 关闭自动登记！我们要在业务真实执行完后，再手动登记到本地小本子上
    conf->set("enable.auto.offset.store", "false", errstr);

    this->consumer_.reset(RdKafka::KafkaConsumer::create(conf, errstr));

    if(!this->consumer_) {
        throw std::runtime_error("Failed to create consumer: " + errstr);
    }

    delete conf;

    RdKafka::ErrorCode err = this->consumer_->subscribe(topics);

    if(err != RdKafka::ERR_NO_ERROR) {
        throw std::runtime_error("Failed to subscribe: " + RdKafka::err2str(err));
    }
}

KafkaConsumer::~KafkaConsumer() {
    this->stop();

    if(this->loopthread_.joinable()) this->loopthread_.join();
}

void KafkaConsumer::start_consuming() {
    while(this->running_) {
        std::unique_ptr<RdKafka::Message> message(this->consumer_->consume(1000));

        switch (message->err()) {
            case RdKafka::ERR__TIMED_OUT:

                break;

            case RdKafka::ERR_NO_ERROR: {
                int32_t  partition_id = message->partition();
                RdKafka::Message* raw_msg = message.release();

                this->OrderedThreadPool_->submit(partition_id, [this, raw_msg] {
                    this->process_message(raw_msg);

                    delete raw_msg;
                });

                break;
            }

            default:
                std::cerr << "Consume failed: " << message->errstr() << std::endl;
                break;
        }
    }
}

void KafkaConsumer::start() {
    this->loopthread_ = std::thread([this] {
        this->start_consuming();
    });
}

static uint64_t getCurrentTimestamp() {
    auto now = std::chrono::system_clock::time_point::clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return milliseconds.count(); //单位是毫秒
}

std::string parseclientjson(Json::Value* root_, std::vector<Message>* messages_, 
    int32_t& userid, const std::string& username, ComputeThreadPool* threadpool) {

    Json::Value root = *root_;
    if(root["payload"].isNull()) {
        // 日志
        return "";
    }
    Json::Value payload = root["payload"];

    if(payload["roomId"].isNull()) {
        // 日志
        return "";
    }
    std::string roomId = payload["roomId"].asString();

    if(payload["messages"].isNull()) {
        // 日志
        return "";
    }
    Json::Value messages = payload["messages"];

    if(messages.isNull() || !messages.isArray()) {
        // 日志
        return "";
    }

    for(int i = 0; i < messages.size(); ++i) {
        Json::Value msg = messages[i];
        Message message;
        if(msg["content"].isNull()) {
            // 日志
            return "";
        }
        message.content = msg["content"].asString();
        message.timestamp = getCurrentTimestamp();
        message.user_id = userid;
        message.username = username;
        messages_->push_back(message);
    }

    return roomId;
}

static std::string SerializeMessageToJson(const Message& msg) {
    Json::Value root;
    root["content"] = msg.content;
    root["userid"] = msg.user_id;
    root["timestamp"] = msg.timestamp;
    root["username"] = msg.username;
    return root.toStyledString();
}

void redisXadd(sw::redis::Redis* redis, std::vector<Message>& messages, 
    const std::string& roomid, ComputeThreadPool* threadpool) {

    for(auto& message : messages) {
        std::string msg_json = SerializeMessageToJson(message);
        std::vector<std::pair<std::string, std::string>> fields{{"payload", msg_json}};

        message.id = redis->xadd(roomid, "*", fields.begin(), fields.end());
    }
}

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

std::string fillserverjson(ComputeThreadPool* threadpool, std::vector<Message>& messages_, std::string& roomid) {
    Json::Value root;
    Json::Value payload;
    Json::Value messages;
    root["type"] = "ServerMessage";
    payload["roomId"] = roomid;

    for(const auto& msg : messages_) {
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
    std::string serverMessage_ = buildWebSocketFrame(writer.write(root));

    return serverMessage_;
}

void KafkaConsumer::stop() { this->running_.store(false); }

void KafkaConsumer::process_message(RdKafka::Message* message) {
    std::string roomid = message->key() ? *message->key() : "";
    std::string data(static_cast<const char*>(message->payload()), message->len());

    Json::Value root;
    Json::Reader reader;

    if(!reader.parse(data, root) || root["type"].isNull()) {

        return;
    }

    std::vector<Message> messages;
    int32_t userid{0};
    std::string username;

    RdKafka::Headers* header = message->headers();

    if(!header) return;

    auto header_list = header->get_all();
    for(const auto& hdr : header_list) {
        if(hdr.key() == "userid") {
            userid = std::stoi(std::string(static_cast<const char*>(hdr.value()), hdr.value_size()));

        } else if(hdr.key() == "username") {
            username.assign(std::string(static_cast<const char*>(hdr.value()), hdr.value_size()));
        }
    }

    // std::string roomid = co_await async_parseclientjson_for_coro(&root, &messages, userid, username, this->ComputeThreadPool_);
    std::string roomid_ = parseclientjson(&root, &messages, userid, username, this->ComputeThreadPool_);

    // bool ok = co_await async_redisXadd_for_coro(this->redis_pool_, messages, roomid, this->ComputeThreadPool_);
    redisXadd(this->redis_pool_, messages, roomid, this->ComputeThreadPool_);

    // std::string resMessage = co_await async_fillserverjson_for_coro(this->ComputeThreadPool_, &messages, roomid);
    std::string resMessage = fillserverjson(this->ComputeThreadPool_, messages, roomid);

    std::string payload_to_publish = std::to_string(userid) + ":" + resMessage;

    this->redis_pool_->publish("room:" + roomid, payload_to_publish);

    RdKafka::Error* err = message->offset_store();

    if (err) {
        std::cerr << "Failed to store offset: " << err->str() << std::endl;
        delete err;
    }
}
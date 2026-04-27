#include "KafkaConsumer.h"
#include <iostream>

#include "coroutineTask.h"
#include "types.h"
#include "yyjson/JsonView.h"
#include "chat_generated.h"

KafkaConsumer::KafkaConsumer(const std::string& brokers, const std::string& group_id, 
    std::vector<std::string> topics, OrderedThreadPool* ordthreadpool, 
    ComputeThreadPool* comthreadpool, sw::redis::Redis* redis_pool) : 
    OrderedThreadPool_(ordthreadpool), ComputeThreadPool_(comthreadpool), redis_pool_(redis_pool), running_(true) {

    std::string errstr;
    RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    conf->set("bootstrap.servers", brokers, errstr);

    conf->set("group.id", group_id, errstr);

    conf->set("auto.offset.reset", "earliest", errstr);
    // conf->set("auto.offset.reset", "latest", errstr);
        
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
    this->lua_sha1_ = this->redis_pool_->script_load(KafkaConsumer::lua_script);

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

void KafkaConsumer::stop() { this->running_.store(false); }

void KafkaConsumer::process_message(RdKafka::Message* message) {
    std::string roomid = message->key() ? *message->key() : "";
    auto rootMsg = ChatApp::GetRootMessage(static_cast<const char*>(message->payload()));

    auto payload = rootMsg->payload_as_ClientMessagePayload();

    std::string_view clientMessageId{payload->client_message_id()->c_str(), payload->client_message_id()->size()};
    std::string_view target_id{payload->target_id()->c_str(), payload->target_id()->size()};
    auto chat_type = payload->chat_type();

    RdKafka::Headers* header = message->headers();

    if(!header) {
        std::cout << "重试" << std::endl;
        return;
    }

    int32_t userid;
    std::string username;

    auto header_list = header->get_all();
    for(const auto& hdr : header_list) {
        if(hdr.key() == "userid") {
            userid = std::stoi(std::string(static_cast<const char*>(hdr.value()), hdr.value_size()));

        } else if(hdr.key() == "username") {
            username.assign(std::string(static_cast<const char*>(hdr.value()), hdr.value_size()));
        }
    }

    // std::string session_id = chat_type == ChatApp::ChatType::ChatType_Group ? target_id : ;

    std::vector<std::string> keys{"client_msg_set:{" + roomid + "}", "room_seq:{" + roomid + "}"};
    std::vector<std::string> args{std::string(clientMessageId.data(), clientMessageId.size()), "1"};

    std::vector<long long> returned_msgids;
    this->redis_pool_->evalsha(this->lua_sha1_, keys.begin(), keys.end(), args.begin(), args.end(), std::back_inserter(returned_msgids));

    if(returned_msgids.empty()) {
        std::vector<std::pair<Message, long long>> missMessages;

        // std::string resMessage = fillserverjson(this->ComputeThreadPool_, missMessages, clientMessageId, roomid);

        // this->redis_pool_->publish("room:" + roomid, resMessage);
        return ;
    }

    thread_local flatbuffers::FlatBufferBuilder builder(4096);
    builder.Clear();

    // std::vector<flatbuffers::Offset<ChatApp::ServerMessageItem>> serverMsgItemOffsets;
    auto username_flat = builder.CreateString(username);

    ChatApp::UserBuilder UserBuilder(builder);
    UserBuilder.add_userid(userid);
    UserBuilder.add_username(username_flat);
    auto UserOffset = UserBuilder.Finish();

    auto content = payload->messages()->content();
    auto client_message_id = builder.CreateString(clientMessageId.data());
    auto content_flat = builder.CreateString(content->data());

    ChatApp::ServerMessageItemBuilder serverMsgItemBuilder(builder);
    serverMsgItemBuilder.add_user(UserOffset);
    serverMsgItemBuilder.add_content(content_flat);
    serverMsgItemBuilder.add_server_message_id(returned_msgids[0]);
    serverMsgItemBuilder.add_client_message_id(client_message_id);
    serverMsgItemBuilder.add_timestamp(getCurrentTimestamp());
    auto serverMsgItemOffset = serverMsgItemBuilder.Finish();

    auto room_id_flat = builder.CreateString(roomid);

    ChatApp::ServerMessagePayloadBuilder serverMsgPayloadBuilder(builder);
    // serverMsgPayloadBuilder.add_room_id(room_id_flat);
    serverMsgPayloadBuilder.add_messages(serverMsgItemOffset);
    auto serverMsgOffset = serverMsgPayloadBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_ServerMessagePayload);
    rootMsgBuilder.add_payload(serverMsgOffset.Union());
    builder.Finish(rootMsgBuilder.Finish());

    const char* fb_data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    size_t fb_size = builder.GetSize();
    std::string_view fb_binary(fb_data, fb_size);

    try{
        auto pipe = this->redis_pool_->pipeline();

        std::vector<std::pair<std::string, std::string_view>> stream_fields{
            {"payload", fb_binary}
        };

        long long max_seq_id = returned_msgids.back();
        std::string stream_id = std::to_string(max_seq_id) + "-0";

        pipe.xadd("{" + roomid + "}", stream_id, stream_fields.begin(), stream_fields.end(), 500);

        pipe.publish("room:" + roomid, fb_binary);

        pipe.exec();

    } catch(const sw::redis::Error& e) {
        std::cerr << "Redis Pipeline 执行失败: " << e.what() << std::endl;
        return;
    }

    RdKafka::Error* err = message->offset_store();

    if (err) {
        std::cerr << "Failed to store offset: " << err->str() << std::endl;
        delete err;
    }
}
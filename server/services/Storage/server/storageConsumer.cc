#include "storageConsumer.h"

#include "storage/AsyncMySQLConnPool/AsyncMysqlConnPool.h"
#include "storage/AsyncMySQLConnPool/AsyncMysqlConn.h"
#include "storage/AsyncMySQLConnPool/AsioBridge.h"
#include "storage/AsyncMySQLConnPool/AsyncMysqlCluster.h"

#include "concurrency/coroutineTask.h"
#include <iostream>

#include "chat_generated.h"
#include "SnowflakeIdWorker.h"

storageConsumer::storageConsumer(asyncMysqlCluster* pool, const std::string& brokers, 
    const std::string& group_id, const std::vector<std::string>& topics) : 
    mysql_pool_(pool), running_(true) {

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

    RdKafka::ErrorCode err = this->consumer_->subscribe(std::move(topics));

    if(err != RdKafka::ERR_NO_ERROR) {
        throw std::runtime_error("Failed to subscribe: " + RdKafka::err2str(err));
    }
}

static DetachedTask DoLandingMysql(asyncMysqlConnPool* pool, RdKafka::Message* raw_msg) {
    auto conn = co_await AwaitAsio(pool->get_ioc(), pool->Acquire());
    mysqlConnGuard guard(pool, conn);

    std::string kafka_key = raw_msg->key() ? *raw_msg->key() : "";
    RdKafka::Headers* header = raw_msg->headers();

    if(!header) co_return;

    std::string sender_id_str; std::string sender_name;

    auto header_list = header->get_all();
    for(const auto& hdr : header_list) {
        if(hdr.key() == "userid") {
            sender_id_str = std::string(static_cast<const char*>(hdr.value()), hdr.value_size());

        } else if(hdr.value() == "username") {
            sender_name = std::string(static_cast<const char*>(hdr.value()), hdr.value_size());
        }
    }

    auto rootMsg = ChatApp::GetRootMessage(static_cast<const char*>(raw_msg->payload()));
    auto payload = rootMsg->payload_as_ClientMessagePayload();

    std::string_view client_msg_id(payload->client_message_id()->c_str(), payload->client_message_id()->size());

    std::string sql;

    co_await AwaitAsio(pool->get_ioc(), guard->execute(sql));

    delete raw_msg;
}

void storageConsumer::start() {
    while(this->running_) {
        std::unique_ptr<RdKafka::Message> message(this->consumer_->consume(1000));

        switch (message->err()) {
            case RdKafka::ERR__TIMED_OUT:

                break;

            case RdKafka::ERR_NO_ERROR: {
                int32_t partition_id = message->partition();
                RdKafka::Message* raw_msg = message.release();

                int index = std::hash<int32_t>{}(partition_id) % this->mysql_pool_->get_thread_count();
                asyncMysqlConnPool* pool = this->mysql_pool_->get_index_pool(index);

                DoLandingMysql(pool, raw_msg);

                break;
            }

            default:
                std::cerr << "Consume failed: " << message->errstr() << std::endl;
                break;
        }
    }
}
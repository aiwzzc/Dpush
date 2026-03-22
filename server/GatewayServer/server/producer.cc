#include "producer.h"
#include "../../base/JsonView.h"

kafkaProducer::kafkaProducer() : my_dr_cb_(std::make_unique<MyDeliveryReportCb>()) { this->init_kafka(); }
kafkaProducer::~kafkaProducer() = default;

/*
{
  "type": "MessageAck",
  "payload": {
    "clientMessageId": "msg_1710000000000_a1b2c3d",
    "status": "SUCCESS"
  }
}
*/

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

std::string packet_ack(const std::string& msg_id, const std::string& status) {
    JsonDoc root;

    root.root()["type"].set("MessageAck");
    auto payload = root.root()["payload"];
    payload["clientMessageId"].set(msg_id);
    payload["status"].set(status);

    return buildWebSocketFrame(root.toString());
}

class MyDeliveryReportCb : public RdKafka::DeliveryReportCb {
    void dr_cb(RdKafka::Message& message) override {
        auto* ctx = static_cast<KafkaDeliveryContext*>(message.msg_opaque());

        bool success = (message.err() == RdKafka::ERR_NO_ERROR);

        TcpConnectionPtr conn = ctx->conn_.lock();

        if(conn) {
            std::string ack_data = success ? packet_ack(ctx->msg_id_, "SUCCESS") : packet_ack(ctx->msg_id_, "FAILED");

            conn->send(ack_data);

        } else {

        }

        delete ctx;
    }
};

void kafkaProducer::init_kafka() {
    std::string errstr;
    RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    conf->set("bootstrap.servers", "127.0.0.1:9092", errstr);

    // 1. 开启幂等性生产（Kafka 0.11+ 支持）
    // 开启后，librdkafka 会为每条消息自动加上 Sequence Number。
    // 就算底层因为网络原因重试，Kafka Broker 也能靠序列号去重并保证绝对的严格顺序！
    conf->set("enable.idempotence", "true", errstr);

    // 2. 如果开启了幂等性，其实下面这个参数默认就是安全的。
    // 但为了确保万无一失，我们显式设置最大在途请求数不超过 5（官方推荐）。
    conf->set("max.in.flight.requests.per.connection", "5", errstr);

    // 3. 必须等待所有副本写入（保证可靠性不丢消息）
    conf->set("acks", "all", errstr);

    // 4. 高性能凑批参数（提升吞吐）
    conf->set("linger.ms", "5", errstr);
    conf->set("batch.num.messages", "10000", errstr);

    conf->set("dr_cb", this->my_dr_cb_.get(), errstr);

    RdKafka::Producer* raw_producer = RdKafka::Producer::create(conf, errstr);

    if(!raw_producer) {
        throw std::runtime_error("Failed to create Kafka producer: " + errstr);
    }

    this->producer_.reset(raw_producer);

    delete conf;
}

// RdKafka::ErrorCode kafkaProducer::produce(const std::string& topic, char* data, std::size_t data_len, 
//     const std::string& key, std::size_t key_len, void* ctx, int32_t userid, std::string& username) {
    
//     RdKafka::Headers* header = RdKafka::Headers::create();

//     header->add("userid", std::to_string(userid));
//     header->add("username", username);

//     RdKafka::ErrorCode err = this->producer_->produce(topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
//     const_cast<char*>(data), data_len, key.c_str(), key_len, 0, header, ctx);

//     return err;
// }

RdKafka::ErrorCode kafkaProducer::produce(const std::string& topic, const char* data, std::size_t data_len, 
    const std::string& key, std::size_t key_len, void* ctx, int32_t userid, std::string& username) {
    
    RdKafka::Headers* header = RdKafka::Headers::create();

    header->add("userid", std::to_string(userid));
    header->add("username", username);

    RdKafka::ErrorCode err = this->producer_->produce(topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
    const_cast<char*>(data), data_len, key.c_str(), key_len, 0, header, ctx);

    return err;
}

void kafkaProducer::poll(int timeout) {
    if(this->producer_) {
        this->producer_->poll(timeout);
    }
}
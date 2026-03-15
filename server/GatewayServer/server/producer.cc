#include "producer.h"

kafkaProducer::kafkaProducer() : my_dr_cb_(std::make_unique<MyDeliveryReportCb>()) { this->init_kafka(); }
kafkaProducer::~kafkaProducer() = default;

struct KafkaDeliveryContext {
    std::weak_ptr<WebsocketConn> conn_;
    std::string msg_id_;
};

std::string packet_ack(const std::string& msg_id, std::string_view status) {
    return "";
}

class MyDeliveryReportCb : public RdKafka::DeliveryReportCb {
    void dr_cb(RdKafka::Message& message) override {
        auto* ctx = static_cast<KafkaDeliveryContext*>(message.msg_opaque());

        bool success = (message.err() == RdKafka::ERR_NO_ERROR);

        WebsocketConnPtr conn = ctx->conn_.lock();

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

RdKafka::ErrorCode kafkaProducer::produce(const std::string& topic, char* data, std::size_t data_len, 
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
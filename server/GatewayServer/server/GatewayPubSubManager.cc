#include "GatewayPubSubManager.h"
#include "websocketConn.h"

std::unordered_map<int32_t, WebsocketConnPtr> GatewayPubSubManager::WebsockConnhash{};
std::mutex GatewayPubSubManager::WebsockConnhashMutex;

std::unordered_map<std::string, std::unordered_set<WebsocketConnPtr>> GatewayPubSubManager::WebsockConnRoomhash{};
std::mutex GatewayPubSubManager::WebsockConnRoomhashMutex;

std::vector<std::string> GatewayPubSubManager::pending_sub_channels_{};
std::vector<std::string> GatewayPubSubManager::pending_unsub_channels_{};
std::mutex GatewayPubSubManager::channel_mtx_;

GatewayPubSubManager::GatewayPubSubManager() {
    sw::redis::ConnectionOptions opts;
    opts.host = "127.0.0.1";
    opts.port = 6379;
    opts.db = 1;
    opts.socket_timeout = std::chrono::milliseconds(100); // 100毫秒超时

    auto redis_for_sub = sw::redis::Redis(opts);
    this->sub_ = std::make_unique<sw::redis::Subscriber>(redis_for_sub.subscriber());

    this->sub_->on_message([this] (std::string channel, std::string msg) {
        std::string roomid = channel;
        if(roomid.rfind("room:", 0) == 0) roomid = channel.substr(5);

        std::size_t colon_pos = msg.find(":");
        if(colon_pos == std::string::npos) {
            // 日志
            return;
        }

        int32_t sender_userid = 0;
        try {
            sender_userid = std::stoi(msg.substr(0, colon_pos));

        } catch(...) { return; }

        std::string actual_msg = msg.substr(colon_pos + 1);

        std::lock_guard<std::mutex> lock(GatewayPubSubManager::WebsockConnRoomhashMutex);

        auto it = GatewayPubSubManager::WebsockConnRoomhash.find(roomid);
        if(it != GatewayPubSubManager::WebsockConnRoomhash.end()) {
            std::unordered_set<WebsocketConnPtr>& conns = it->second;

            for(const auto& conn : conns) {
                if(conn->userid() == sender_userid) continue;

                conn->send(actual_msg);
            }
        }
    });

    this->consume_thread_ = std::thread(&GatewayPubSubManager::ConsumLoop, this);
}

GatewayPubSubManager::~GatewayPubSubManager() {
    this->running_ = false;
    if(this->consume_thread_.joinable()) this->consume_thread_.join();
}

void GatewayPubSubManager::SubscribeRoomSafe(const std::string& roomid) {
    std::lock_guard<std::mutex> lock(GatewayPubSubManager::channel_mtx_);
    GatewayPubSubManager::pending_sub_channels_.push_back("room:" + roomid);
}

void GatewayPubSubManager::UnSubscribeRoomSafe(const std::string& roomid) {
    std::lock_guard<std::mutex> lock(GatewayPubSubManager::channel_mtx_);
    GatewayPubSubManager::pending_unsub_channels_.push_back("room:" + roomid);
}

void GatewayPubSubManager::ConsumLoop() {
    while(1) {
        try {
            std::vector<std::string> sublist;
            std::vector<std::string> unsublist;

            {
                std::lock_guard<std::mutex> lock(GatewayPubSubManager::channel_mtx_);
                sublist = std::move(GatewayPubSubManager::pending_sub_channels_);
                unsublist = std::move(GatewayPubSubManager::pending_unsub_channels_);
            }

            if(!sublist.empty()) this->sub_->subscribe(sublist.begin(), sublist.end());

            if(!unsublist.empty()) this->sub_->unsubscribe(unsublist.begin(), unsublist.end());

            this->sub_->consume();

        } catch(const sw::redis::TimeoutError &e) {

            continue;

        } catch(const std::exception &e) {
            std::cerr << "Redis Subscriber Error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}
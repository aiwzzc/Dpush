#include "GatewayPubSubManager.h"
#include "websocketConn.h"

std::unordered_map<int32_t, WebsocketConnPtr> GatewayPubSubManager::WebsockConnhash{};
std::mutex GatewayPubSubManager::WebsockConnhashMutex{};

std::array<RoomBucket, BUCKET_NUM> GatewayPubSubManager::roomBuckets{};

// 无锁优化
thread_local std::unordered_map<int32_t, WebsocketConnPtr> LocalWebsockConnhash;
thread_local std::unordered_map<std::string, std::unordered_set<WebsocketConnPtr>> LocalWebsockConnRoomhash;

std::vector<std::string> GatewayPubSubManager::pending_sub_channels_{};
std::vector<std::string> GatewayPubSubManager::pending_unsub_channels_{};
std::unordered_map<std::string, int32_t> GatewayPubSubManager::global_room_ref_count_{};
std::mutex GatewayPubSubManager::channel_mtx_{};

std::vector<EventLoop*> GatewayPubSubManager::all_io_loops_{};
std::mutex GatewayPubSubManager::loops_mtx_{};

LRUCache<int32_t, std::vector<std::string>> GatewayPubSubManager::UserRoomLRU_{10000};

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

#if 0
        std::size_t bucketIndex = std::hash<std::string>{}(roomid) % BUCKET_NUM;
        auto& bucket = GatewayPubSubManager::roomBuckets[bucketIndex];

        std::lock_guard<std::mutex> lock(bucket.mtx_);

        auto& conns = bucket.roomHash_[roomid];
        if(conns.empty()) return;

        for(const auto& conn : conns) {
            conn->send(msg);
        }
#elif 1
        for(EventLoop* loop : GatewayPubSubManager::all_io_loops_) {
            loop->runInLoop([roomid, msg] () {
                auto it = LocalWebsockConnRoomhash.find(roomid);

                if(it != LocalWebsockConnRoomhash.end()) {
                    for(const auto& conn : it->second) {
                        conn->send(msg);
                    }
                }
            });
        }

#endif

    });

    this->consume_thread_ = std::thread(&GatewayPubSubManager::ConsumLoop, this);
}

GatewayPubSubManager::~GatewayPubSubManager() {
    this->running_ = false;
    if(this->consume_thread_.joinable()) this->consume_thread_.join();
}

void GatewayPubSubManager::RegisterLoop(EventLoop* loop) {
    std::lock_guard<std::mutex> lock(GatewayPubSubManager::loops_mtx_);
    GatewayPubSubManager::all_io_loops_.push_back(loop);
}

// 幂等性设计
void GatewayPubSubManager::SubscribeRoomSafe(const std::string& roomid) {
    std::lock_guard<std::mutex> lock(GatewayPubSubManager::channel_mtx_);
    
    if(++GatewayPubSubManager::global_room_ref_count_[roomid] == 1) {
        GatewayPubSubManager::pending_sub_channels_.push_back("room:" + roomid);
    }
}

void GatewayPubSubManager::UnSubscribeRoomSafe(const std::string& roomid) {
    std::lock_guard<std::mutex> lock(GatewayPubSubManager::channel_mtx_);

    auto it = GatewayPubSubManager::global_room_ref_count_.find(roomid);

    if(it != GatewayPubSubManager::global_room_ref_count_.end()) {
        if(--it->second == 0) {
            GatewayPubSubManager::pending_unsub_channels_.push_back("room:" + roomid);
            GatewayPubSubManager::global_room_ref_count_.erase(it);
        }
    }
}

void GatewayPubSubManager::ConsumLoop() {
    while(this->running_) {
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
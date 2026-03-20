#pragma once

#include <thread>
#include <sw/redis++/redis++.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <array>
#include <unordered_map>
#include <unordered_set>

#include "muduo/net/EventLoop.h"
#include "LRUCache.h"

class WebsocketConn;
using WebsocketConnPtr = std::shared_ptr<WebsocketConn>;
using muduo::net::EventLoop;

constexpr std::size_t BUCKET_NUM = 16;

// hash 分桶
struct RoomBucket {
    std::mutex mtx_;
    std::unordered_map<std::string, std::unordered_set<WebsocketConnPtr>> roomHash_;
};

class GatewayPubSubManager {

public:
    // userid --> websocketconn
    static std::unordered_map<int32_t, WebsocketConnPtr> WebsockConnhash;
    static std::mutex WebsockConnhashMutex;

    static std::array<RoomBucket, BUCKET_NUM> roomBuckets;

    static std::vector<std::string> pending_sub_channels_;
    static std::vector<std::string> pending_unsub_channels_;

    // 全局订阅房间引用计数
    static std::unordered_map<std::string, int32_t> global_room_ref_count_;
    static std::mutex channel_mtx_;

    static std::vector<EventLoop*> all_io_loops_;
    static std::mutex loops_mtx_;

    static LRUCache<int32_t, std::vector<std::string>> UserRoomLRU_; 

    GatewayPubSubManager();
    ~GatewayPubSubManager();

    static void SubscribeRoomSafe(const std::string& roomid);
    static void UnSubscribeRoomSafe(const std::string& roomid);
    static void RegisterLoop(EventLoop* loop);

private:
    void ConsumLoop();

    std::unique_ptr<sw::redis::Subscriber> sub_;
    std::thread consume_thread_;
    std::atomic<bool> running_{true};
};

extern thread_local std::unordered_map<int32_t, WebsocketConnPtr> LocalWebsockConnhash;
extern thread_local std::unordered_map<std::string, std::unordered_set<WebsocketConnPtr>> LocalWebsockConnRoomhash;
#pragma once

#include <thread>
#include <sw/redis++/redis++.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

class WebsocketConn;
using WebsocketConnPtr = std::shared_ptr<WebsocketConn>;

class GatewayPubSubManager {

public:
    // userid --> websocketconn
    static std::unordered_map<int32_t, WebsocketConnPtr> WebsockConnhash;
    static std::mutex WebsockConnhashMutex;

    // roomid --> websocketconn
    static std::unordered_map<std::string, std::unordered_set<WebsocketConnPtr>> WebsockConnRoomhash;
    static std::mutex WebsockConnRoomhashMutex;

    static std::vector<std::string> pending_sub_channels_;
    static std::vector<std::string> pending_unsub_channels_;
    static std::mutex channel_mtx_;

    GatewayPubSubManager();
    ~GatewayPubSubManager();

    static void SubscribeRoomSafe(const std::string& roomid);
    static void UnSubscribeRoomSafe(const std::string& roomid);

private:
    void ConsumLoop();

    std::unique_ptr<sw::redis::Subscriber> sub_;
    std::thread consume_thread_;
    std::atomic<bool> running_{true};
};
#pragma once

#include <memory>
#include <vector>
#include <unordered_set>
#include <functional>
#include <string>
#include <string_view>

#include "websocketSession.h"

class Entry {

public:
    explicit Entry(const WsSessionPtr&);
    ~Entry();

private:
    std::weak_ptr<WsSession> conn_;

};

using EntryPtr = std::shared_ptr<Entry>;
using Bucket = std::unordered_set<EntryPtr>;
using TimingWheel = std::vector<Bucket>;

class heartbeatManager {

public:
    heartbeatManager();

    void onTimerTick();
    void onMessagePing(const WsSessionPtr& webconn, int64_t ts, 
        const std::function<void(const std::string&)>& callback);

private:
    TimingWheel wheel_;
    int current_index_;

};
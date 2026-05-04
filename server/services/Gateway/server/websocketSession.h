#pragma once

#include "muduo/net/TcpConnection.h"
#include "muduo/net/EventLoop.h"
#include "types.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <optional>
#include <sw/redis++/redis++.h>
#include <any>

class HttpRequest;

using muduo::net::TcpConnectionPtr;
using muduo::net::EventLoop;

enum class WsState {
    EXPECTING_HTTP_REQUEST = 0,
    ESTABLISHED = 1
};

class WsSession;
using WsSessionPtr = std::shared_ptr<WsSession>;

class WsSession : public std::enable_shared_from_this<WsSession> {

public:
    using WebconnCloseCallback = std::function<void()>;

    WsState state_ = WsState::EXPECTING_HTTP_REQUEST;

    WsSession(const TcpConnectionPtr&);
    ~WsSession();

    std::vector<std::string> onRead(const TcpConnectionPtr& conn, muduo::net::Buffer* buf);
    void setUserid(int32_t userid);
    void setUsername(const std::string&);
    void setWebconnCloseCallback(const WebconnCloseCallback& cb);
    void setjoinedRooms(const std::unordered_map<std::string, int>& rooms);
    void setContext(const std::any& context);
    const std::any& getContext() const;
    std::any* getMutableContext();

    void send(const std::string&);
    void send(const char*, std::size_t);
    void sendPongFrame();
    void sendCloseFrame(uint16_t code, const std::string reason);

    bool connected();
    void disconnect();
    void forceClose();

    EventLoop* getLoop() const;
    std::string& username();
    int32_t userid() const;
    TcpConnectionPtr conn() const;
    void setRoomIndex(const std::string& room_id, int new_index);
    void joinRoom(const std::string& room_id, int new_index);
    std::optional<int> getRoom_index(const std::string& room_id) const;
    const std::unordered_map<std::string, int>& getjoinedRooms() const;

    static void SubscribeSession(const WsSessionPtr& conn, const std::string& roomid);

private:
    bool isCloseFrame();

    std::weak_ptr<muduo::net::TcpConnection> conn_;
    int32_t userid_;
    std::string username_;
    WebconnCloseCallback webconnCloseCallback_;

    std::unordered_map<std::string, int> joinedRooms_;
    std::any context_;

};

extern std::unordered_map<int32_t, WsSessionPtr> WebsockConnhash;
extern std::mutex WebsockConnhashMutex;
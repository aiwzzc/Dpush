#pragma once

#include "muduo/net/TcpConnection.h"
#include "muduo/net/EventLoop.h"
#include "../../base/types.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <sw/redis++/redis++.h>

class HttpRequest;

using muduo::net::TcpConnectionPtr;
using muduo::net::EventLoop;

struct WebSocketFrame {
    bool fin;
    uint8_t opcode;
    bool mask;
    uint64_t payload_length;
    uint8_t masking_key[4];
    std::string payload_data;
};

WebSocketFrame parseWebSocketFrame(const std::string& data);

class WebsocketConn : public std::enable_shared_from_this<WebsocketConn> {

public:
    int room_index_ = -1;

    using WebconnCloseCallback = std::function<void()>;

    WebsocketConn(const TcpConnectionPtr&);
    ~WebsocketConn();

    // std::vector<std::string> onRead(const TcpConnectionPtr& conn, muduo::net::Buffer* buf);
    std::vector<std::string> onRead(const TcpConnectionPtr& conn, muduo::net::Buffer* buf);
    void setUserid(int32_t userid);
    void setUsername(const std::string&);
    void setWebconnCloseCallback(const WebconnCloseCallback& cb);
    void setjoinedRooms(const std::vector<std::string>& rooms);

    void send(const std::string&);
    void send(const char*, std::size_t);
    void sendPongFrame();
    void sendCloseFrame(uint16_t code, const std::string reason);
    void addRoom(const std::string& roomid);

    bool connected();
    void disconnect();

    EventLoop* getLoop() const;
    std::string& username();
    int32_t userid() const;
    TcpConnectionPtr conn() const;
    std::vector<std::string> getjoinedRooms() const;

private:
    bool isCloseFrame();

    TcpConnectionPtr conn_;
    int32_t userid_;
    std::string username_;
    WebconnCloseCallback webconnCloseCallback_;

    std::vector<std::string> joinedRooms_;
};

using WebsocketConnPtr = std::shared_ptr<WebsocketConn>;

extern std::unordered_map<int32_t, WebsocketConnPtr> WebsockConnhash;
extern std::mutex WebsockConnhashMutex;
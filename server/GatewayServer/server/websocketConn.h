#pragma once

#include "muduo/net/TcpConnection.h"
#include "../../base/types.h"
#include "grpcClient.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <sw/redis++/redis++.h>
#include <jsoncpp/json/json.h>

class HttpRequest;

using muduo::net::TcpConnectionPtr;

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
    using WebconnCloseCallback = std::function<void()>;

    WebsocketConn(const TcpConnectionPtr&);
    ~WebsocketConn();

    std::string onRead(const TcpConnectionPtr& conn, muduo::net::Buffer* buf);
    void setUserid(int32_t userid);
    void setUsername(const std::string&);
    void setWebconnCloseCallback(const WebconnCloseCallback& cb);
    void setgrpcClientPtr(grpcClientPtr);

    void send(const std::string&);
    void send(const char*, std::size_t);
    void sendPongFrame();
    void sendCloseFrame(uint16_t code, const std::string reason);

    bool connected();
    void disconnect();

    std::string& username();
    int32_t userid() const;

private:
    bool isCloseFrame();

    TcpConnectionPtr conn_;
    int32_t userid_;
    std::string username_;
    WebconnCloseCallback webconnCloseCallback_;
    grpcClientPtr grpcClient_;
};

using WebsocketConnPtr = std::shared_ptr<WebsocketConn>;

extern std::unordered_map<int32_t, WebsocketConnPtr> WebsockConnhash;
extern std::mutex WebsockConnhashMutex;
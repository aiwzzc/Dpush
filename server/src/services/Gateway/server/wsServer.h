#pragma once

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"

#include <memory>
#include <string>
#include <vector>

#include "GatewayConfig.h"

namespace muduo {
namespace net {
    class InetAddress;
    class Buffer;
};
};

using muduo::net::TcpConnectionPtr;

class wsServer {

public:
    using ThreadInitCallback = std::function<void(muduo::net::EventLoop*)>;
    using MainLoopTimerCallback = std::function<void()>;

    wsServer(
        const muduo::net::InetAddress &addr, 
        const std::string& name, int num_event_loops, 
        const pulse::config::WsServerContext& ctx);
    ~wsServer();

    void start();
    void setThreadInitCallback(const ThreadInitCallback& cb);
    void setMainLoopTimerCallback(const MainLoopTimerCallback& cb);

private:
    constexpr static const char* k2CRLF = "\r\n\r\n";

    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buffer);
    bool processHandshake(const std::string& header, const muduo::net::TcpConnectionPtr& conn);
    void onUpgrade(const muduo::net::TcpConnectionPtr& conn);
    void dispatchEvent(const muduo::net::TcpConnectionPtr& conn, const std::vector<std::string>&);

    std::unique_ptr<muduo::net::EventLoop> loop_;
    std::unique_ptr<muduo::net::TcpServer> tcpServer_;

    pulse::config::WsServerContext wsContext_;

    MainLoopTimerCallback mainLoopTimerCallback_;
};
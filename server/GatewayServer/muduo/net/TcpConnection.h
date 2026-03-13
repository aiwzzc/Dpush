#pragma once

#include <memory>
#include <functional>
#include <string>
#include <any>
#include <atomic>

class Channel;
class EventLoop;
class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, std::string&)>;
using CloseCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {

public:
    explicit TcpConnection(Channel* ch);
    ~TcpConnection();

    int fd() const;
    void send(const std::string& msg);
    void send(const char* msg, std::size_t len);
    void setMessageCallback(const MessageCallback& cb);
    void setCloseCallback(const CloseCallback& cb);

    void setContext(const std::any& context);
    void setContext(std::any&& context);

    bool connected() const;
    bool disconnected() const;
    
    std::any* getMutableContext();
    EventLoop* loop() const;

    void shutdown();

private:
    enum StateE { 
        kConnecting,    // 正在连接 (通常用于客户端)
        kConnected,     // 已连接
        kDisconnecting, // 正在断开 (调用了 shutdown，即半关闭状态)
        kDisconnected   // 已完全断开
    };

    std::atomic<StateE> state_;

    void setState(StateE s);

    void sendInloop(const std::string& msg);
    void shutdownInloop();
    void handleWrite();
    void handleRead();
    void handelClose();

    static constexpr std::size_t BUFFERSIZE = 1024 * 4;
    std::string readBuf_;
    std::string writeBuf_;
    Channel* ch_;
    MessageCallback messagecallback_;
    CloseCallback closecallback_;
    std::any context_;
};
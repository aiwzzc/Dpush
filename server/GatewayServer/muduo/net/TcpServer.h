#pragma once

#include <memory>
#include <functional>
#include <unordered_map>

class TcpConnection;
class EventLoop;
class EventLoopThreadPool;
class Acceptor;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, std::string&)>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;

class TcpServer {

public:
    TcpServer(int subThreadNum);
    ~TcpServer();

    void start();
    void setMessageCallback(const MessageCallback& cb);
    void setConnectionCallback(const ConnectionCallback& cb);
    void removeConnection(const std::shared_ptr<TcpConnection>&);

private:
    void removeConnectionInLoop(const std::shared_ptr<TcpConnection>&);

    MessageCallback messagecallback_;
    ConnectionCallback connectionCallback_;
    std::unique_ptr<EventLoop> mainLoop_;
    std::unique_ptr<EventLoopThreadPool> EventLoopThreadPool_;
    std::unique_ptr<Acceptor> Acceptor_;
    std::unordered_map<int, std::shared_ptr<TcpConnection>> connections_;
};
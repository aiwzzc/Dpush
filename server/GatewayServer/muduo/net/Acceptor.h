#pragma once

#include <functional>

class Channel;
class EventLoop;
using NewConnectionCallback = std::function<void(int)>;

class Acceptor {

public:
    Acceptor(EventLoop* loop);
    ~Acceptor();

    void start();
    void setNewConnectionCallback(const NewConnectionCallback& cb);

private:
    void acceptNewConnection();

    Channel* listench_;
    NewConnectionCallback newconnectioncallback_;
};
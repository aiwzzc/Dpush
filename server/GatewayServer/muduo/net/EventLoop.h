#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <sys/eventfd.h>

class Epoller;
class Channel;

class EventLoop {

public:
    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    bool isInLoopThread() const;
    void runInLoop(std::function<void()> cb);
    void queueInLoop(std::function<void()> cb);

    Epoller* epoller() const;

private:
    void wakeup();
    void doPendingFunctors();

    Epoller* epoller_;
    std::thread::id threadId_;
    bool quit_;

    int wakeupfd_;
    Channel* wakeupChannel_;

    std::mutex mutex_;
    std::queue<std::function<void()>> dispatchQueue_;

};
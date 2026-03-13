#pragma once

#include <thread>
#include <vector>

class EventLoop;

class EventLoopThreadPool {

public:
    EventLoopThreadPool(int subThreadNum);
    ~EventLoopThreadPool();

    void start();
    EventLoop* getNextLoop(EventLoop* baseloop);
    std::size_t threadNum() const;

private:
    std::size_t threadNum_;
    std::vector<std::pair<std::thread, EventLoop*>> pool_;
    int index_;
    std::mutex mutex_;
};
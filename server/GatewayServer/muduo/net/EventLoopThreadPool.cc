#include "EventLoopThreadPool.h"
#include "EventLoop.h"

#include <future>

EventLoopThreadPool::EventLoopThreadPool(int subThreadNum) : threadNum_(subThreadNum < 0 ? 0 : subThreadNum), index_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {
    for(auto& [thread, eventloop] : this->pool_) {
        eventloop->quit();
        if(thread.joinable()) thread.join();
        delete eventloop;
    }
}

std::size_t EventLoopThreadPool::threadNum() const { return this->threadNum_; }

void EventLoopThreadPool::start() {
    for(int i = 0; i < this->threadNum_; ++i) {
        std::promise<EventLoop*> promise;
        std::future<EventLoop*> future = promise.get_future();

        std::thread t{[&promise] {
            EventLoop* loop = new EventLoop();
            promise.set_value(loop);
            loop->loop();
        }};

        EventLoop* eventloop = future.get();
        this->pool_.emplace_back(std::move(t), eventloop);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop(EventLoop* baseloop) {
    if(this->pool_.empty()) return baseloop;

    std::lock_guard<std::mutex> lock(this->mutex_);
    EventLoop* nextloop = this->pool_[this->index_].second;
    this->index_ = (this->index_ + 1) % this->threadNum_;

    return nextloop;
}
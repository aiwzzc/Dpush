#include "EventLoop.h"
#include "Channel.h"
#include "Poller.h"

namespace {

int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    if(evtfd < 0) return -1;

    return evtfd;
}

};

EventLoop::EventLoop() : epoller_(new Epoller()), threadId_(std::this_thread::get_id()), quit_(false),
    wakeupfd_(createEventfd()), wakeupChannel_(new Channel(this, wakeupfd_)) {
    this->wakeupChannel_->setReadCallback([this] {
        uint64_t one = 1;
        ::read(this->wakeupfd_, &one, sizeof(one));
    });
    
    this->wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    ::close(this->wakeupfd_);
    delete this->epoller_;
    delete this->wakeupChannel_;
}

void EventLoop::loop() {
    while(!this->quit_) {
        std::vector<std::pair<Channel*, int>> channels;
        this->epoller_->poll(-1, channels);

        if(channels.empty()) continue;

        for(auto& [channel, epollevent] : channels) {
            channel->handleEvent(epollevent);
        }

        doPendingFunctors();
    }
}

void EventLoop::quit() { this->quit_ = true; }

bool EventLoop::isInLoopThread() const {
    return this->threadId_ == std::this_thread::get_id();
}

void EventLoop::runInLoop(std::function<void()> cb) {
    if(isInLoopThread()) cb();
    else queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(std::function<void()> cb) {
    {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->dispatchQueue_.push(std::move(cb));
    }

    if(!isInLoopThread()) wakeup();
}

Epoller* EventLoop::epoller() const { return this->epoller_; }

void EventLoop::wakeup() {
    uint64_t one = 1;
    ::write(this->wakeupfd_, &one, sizeof(one));
}

void EventLoop::doPendingFunctors() {
    std::queue<std::function<void()>> functors;
    {
        std::lock_guard<std::mutex> lock(this->mutex_);
        functors.swap(this->dispatchQueue_);
    }

    while(!functors.empty()) {
        auto cb = functors.front();
        functors.pop();
        cb();
    }
}
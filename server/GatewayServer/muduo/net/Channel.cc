#include "Channel.h"
#include "EventLoop.h"
#include "Poller.h"

#include <sys/epoll.h>

Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd), events_(0), index_(-1) {}

int Channel::fd() const { return this->fd_; }
uint32_t Channel::events() const { return this->events_; }

int Channel::index() const { return this->index_; }
void Channel::setIndex(int idx) { this->index_ = idx; }
EventLoop* Channel::loop() const { return this->loop_; }

void Channel::setReadCallback(std::function<void()> cb) { this->readhandle_ = std::move(cb); }
void Channel::setWriteCallback(std::function<void()> cb) { this->writehandle_ = std::move(cb); }

void Channel::enableReading() { this->events_ |= EPOLLIN; update(); }
void Channel::enableWriting() { this->events_ |= EPOLLOUT; update(); }
void Channel::disableWriting() { this->events_ &= ~EPOLLOUT; update(); }

void Channel::handleEvent(int events) {
    if(events & EPOLLIN) this->readhandle_();
    if(events & EPOLLOUT) this->writehandle_();
}

void Channel::update() { this->loop_->epoller()->updateChannel(this); }
#pragma once

#include <functional>
#include <cstdint>

class EventLoop;

class Channel {

public:
    explicit Channel(EventLoop* loop, int fd);

    int fd() const;
    uint32_t events() const;
    EventLoop* loop() const;
    int index() const;
    void setIndex(int idx);

    void setReadCallback(std::function<void()> cb);
    void setWriteCallback(std::function<void()> cb);

    void enableReading();
    void enableWriting();
    void disableWriting();

    void handleEvent(int events);

private:
    void update();

    EventLoop* loop_;
    int fd_;
    uint32_t events_;
    std::function<void()> readhandle_;
    std::function<void()> writehandle_;
    int index_;
};
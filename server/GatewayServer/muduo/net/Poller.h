#pragma once

#include <sys/epoll.h>
#include <vector>

class Channel;

class Epoller {

public:
    Epoller();
    ~Epoller();

    void poll(int timeout, std::vector<std::pair<Channel*, int>>& channels);

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    static constexpr std::size_t EpollEventSize = 128;
    int epfd_;
    epoll_event evs[EpollEventSize];
};
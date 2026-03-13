#include "Poller.h"
#include "Channel.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>

Epoller::Epoller() : epfd_(epoll_create1(EPOLL_CLOEXEC)) {}
Epoller::~Epoller() { ::close(this->epfd_); }

void Epoller::poll(int timeout, std::vector<std::pair<Channel*, int>>& channels) {
    int nready = epoll_wait(this->epfd_, evs, this->EpollEventSize, timeout);

    for(int i = 0; i < nready; ++i) {
        uint32_t ev = this->evs[i].events;
        channels.push_back({(Channel*)this->evs[i].data.ptr, ev});
    }
}

void Epoller::updateChannel(Channel* channel) {
    epoll_event ev;
    ev.data.ptr = channel;
    ev.events = channel->events();
    
    int fd = channel->fd();
    int index = channel->index();

    if(index == -1 || index == 2) {
        channel->setIndex(1);
        if(epoll_ctl(this->epfd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            perror("epoll_ctl ADD failed");
        }

    } else {
        if(epoll_ctl(this->epfd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
            perror("epoll_ctl MOD failed");
        }
    }
}

void Epoller::removeChannel(Channel* channel) {
    epoll_ctl(this->epfd_, EPOLL_CTL_DEL, channel->fd(), nullptr);
}
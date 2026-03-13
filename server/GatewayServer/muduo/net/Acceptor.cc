#include "Acceptor.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Poller.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>

namespace {

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

};

Acceptor::Acceptor(EventLoop* loop) {
    int listenfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(listenfd < 0) this->listench_ = nullptr;
    else {
        set_nonblocking(listenfd);

        int reuse = 1;
	    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

        sockaddr_in serveraddr{};
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(5005);
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

        if(::bind(listenfd, (sockaddr*)(&serveraddr), sizeof(serveraddr)) < 0) {
            perror("bind failed");
            return;
        }

        this->listench_ = new Channel{loop, listenfd};
        this->listench_->setReadCallback([this] { acceptNewConnection(); });
    }
}

Acceptor::~Acceptor() {
    this->listench_->loop()->epoller()->removeChannel(this->listench_);
    ::close(this->listench_->fd());

    delete this->listench_;
}

void Acceptor::start() {
    ::listen(this->listench_->fd(), SOMAXCONN);
    
    this->listench_->enableReading();
}

void Acceptor::acceptNewConnection() {
    sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(sockaddr);

    while(1) {
        int connfd = accept4(this->listench_->fd(), (sockaddr*)&clientaddr, &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if(connfd < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) break; // 处理完毕
            if(errno == EINTR) continue; // 信号中断
            return; // 其他错误
        }

        if(this->newconnectioncallback_) this->newconnectioncallback_(connfd);
        else ::close(connfd);
    }
}

void Acceptor::setNewConnectionCallback(const NewConnectionCallback& cb) { this->newconnectioncallback_ = std::move(cb); }
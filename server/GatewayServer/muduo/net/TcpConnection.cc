#include "TcpConnection.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Poller.h"

#include <sys/socket.h>
#include <sys/epoll.h>

TcpConnection::TcpConnection(Channel* ch) : state_(StateE::kConnected), ch_(ch) {
    this->ch_->setReadCallback([this] { handleRead(); });
    this->ch_->setWriteCallback([this] { handleWrite(); });
}

TcpConnection::~TcpConnection() { delete this->ch_; }

void TcpConnection::send(const std::string& msg) {
    if(this->ch_->loop()->isInLoopThread()) sendInloop(msg);
    else {
        auto self = shared_from_this();
        this->ch_->loop()->runInLoop([self, msg] { self->sendInloop(msg); });
    }
}

void TcpConnection::send(const char* msg, std::size_t len) {
    std::string msgStr(msg, len);
    if(this->ch_->loop()->isInLoopThread()) sendInloop(msgStr);
    else {
        auto self = shared_from_this();
        this->ch_->loop()->runInLoop([self, msgStr] { self->sendInloop(msgStr); });
    }
}

int TcpConnection::fd() const { return this->ch_->fd(); }

void TcpConnection::setMessageCallback(const MessageCallback& cb) { this->messagecallback_ = std::move(cb); }

void TcpConnection::setCloseCallback(const CloseCallback& cb) { this->closecallback_ = std::move(cb); }

void TcpConnection::setContext(const std::any& context) { this->context_ = context; }
void TcpConnection::setContext(std::any&& context) { this->context_ = std::move(context); }

std::any* TcpConnection::getMutableContext() { return &this->context_; }

EventLoop* TcpConnection::loop() const { return this->ch_->loop(); }

void TcpConnection::sendInloop(const std::string& msg) {
    if(disconnected()) return;

    int nwrote = 0;
    if(this->writeBuf_.empty()) {
        nwrote = ::send(this->ch_->fd(), msg.data(), msg.size(), 0);
        if(nwrote < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) nwrote = 0;
            else {
                // 写入日志
                this->handelClose();
                return;
            }
        }
    }

    if(nwrote < msg.size()) {
        this->writeBuf_.append(msg.substr(nwrote));

        if(!(this->ch_->events() & EPOLLOUT)) this->ch_->enableWriting();
    }
}

void TcpConnection::handleWrite() {
    if(this->ch_->events() & EPOLLOUT) {
        int nwrote = ::send(this->ch_->fd(), this->writeBuf_.data(), this->writeBuf_.size(), 0);
        
        if (nwrote < 0) {
            int e = errno;
            if (e == EAGAIN || e == EWOULDBLOCK) {
                nwrote = 0;
            } else if (e == EPIPE || e == ECONNRESET) {
                handelClose();
                return;
            } else {
                return;
            }
            
        } else {
            this->writeBuf_.erase(0, nwrote);
        }

        if(this->writeBuf_.empty()) this->ch_->disableWriting();
    }
}

void TcpConnection::handleRead() {
    if(this->ch_->events() & EPOLLIN) {
        while(1) {
            char buf[BUFFERSIZE];
            int nread = ::recv(this->ch_->fd(), buf, BUFFERSIZE, 0);

            if(nread > 0) {
                this->readBuf_.append(buf, nread);

            } else if(nread == 0) {
                this->handelClose();
                break;

            } else break;
        }

        if(this->messagecallback_) messagecallback_(shared_from_this(), this->readBuf_);
    }
}

void TcpConnection::handelClose() {
    setState(StateE::kDisconnected);
    this->ch_->loop()->epoller()->removeChannel(this->ch_);

    if(this->closecallback_) this->closecallback_(shared_from_this());
}

void TcpConnection::shutdown() {
    StateE expected = StateE::kConnected;
    if(this->state_.compare_exchange_strong(expected, StateE::kDisconnecting)) {
        if(this->ch_->loop()->isInLoopThread()) shutdownInloop();
        else {
            this->ch_->loop()->runInLoop([this] {
                shutdownInloop();
            });
        }
    }
}

void TcpConnection::shutdownInloop() {
    if(this->writeBuf_.empty()) ::shutdown(this->ch_->fd(), SHUT_WR);
}

void TcpConnection::setState(StateE s) { state_ = s; }

bool TcpConnection::connected() const { return this->state_ == StateE::kConnected; }

bool TcpConnection::disconnected() const { return this->state_ == StateE::kDisconnected; }
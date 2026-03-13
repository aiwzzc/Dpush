#include "TcpServer.h"
#include "Acceptor.h"
#include "Channel.h"
#include "EventLoopThreadPool.h"
#include "TcpConnection.h"
#include "EventLoop.h"

TcpServer::TcpServer(int subThreadNum) : mainLoop_(std::make_unique<EventLoop>()), 
EventLoopThreadPool_(std::make_unique<EventLoopThreadPool>(subThreadNum)), Acceptor_(std::make_unique<Acceptor>(this->mainLoop_.get())) {
    this->Acceptor_->setNewConnectionCallback([this] (int fd) {
        EventLoop* ioLoop = this->EventLoopThreadPool_->getNextLoop(this->mainLoop_.get());

        Channel* connch = new Channel{ioLoop, fd};
        auto newconn = std::make_shared<TcpConnection>(connch);

        newconn->setCloseCallback([this] (const std::shared_ptr<TcpConnection>& conn) {
            this->removeConnection(conn);
        });

        if(this->connectionCallback_) connectionCallback_(newconn);
        if(this->messagecallback_) newconn->setMessageCallback(this->messagecallback_);

        this->connections_[fd] = newconn;

        ioLoop->runInLoop([connch] { connch->enableReading(); });
    });
}

TcpServer::~TcpServer() = default;

void TcpServer::setMessageCallback(const MessageCallback& cb) { this->messagecallback_ = std::move(cb); }

void TcpServer::setConnectionCallback(const ConnectionCallback& cb) { this->connectionCallback_ = std::move(cb); }

void TcpServer::start() {
    this->EventLoopThreadPool_->start();
    this->Acceptor_->start();

    this->mainLoop_->loop();
}

void TcpServer::removeConnection(const std::shared_ptr<TcpConnection>& conn) {
    this->mainLoop_->runInLoop([this, conn] { this->removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(const std::shared_ptr<TcpConnection>& conn) { this->connections_.erase(conn->fd()); }
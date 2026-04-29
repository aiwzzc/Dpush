#include "grpcServer.h"
#include <string>
#include <unordered_map>

#include "muduo/net/EventLoop.h"
#include "websocketConn.h"
#include "GatewayServer.h"

extern thread_local std::unordered_map<int32_t, WebsocketConnPtr> LocalWebsockConnhash;

struct SendSingleMsgAwaiter {

    int32_t user_id_;
    std::string message_;
    muduo::net::EventLoop* loop_;

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {
        this->loop_->runInLoop([this, handle] () {
            auto it = LocalWebsockConnhash.find(this->user_id_);
            if(it != LocalWebsockConnhash.end()) {
                WebsocketConnPtr conn = it->second;

                conn->send(this->message_);
            }

            handle.resume();
        });
    }

    void await_resume() {}

};

static SendSingleMsgAwaiter async_SendSingleMsg_for_coro(int32_t& user_id, const std::string& msg, 
    muduo::net::EventLoop* loop) {

    return SendSingleMsgAwaiter{user_id, std::move(msg), loop};
}

DetachedTask GatewayGrpcServer::DosendSingleMsg(grpc::ServerUnaryReactor* reactor, const gateway::sendSingleMsgRequest* request, 
    gateway::sendSingleMsgResponse* response) {

    response->set_ok(true);

    int32_t user_id = request->user_id();
    std::string message = request->message();

    muduo::net::EventLoop* loop = nullptr;

    {
        std::shared_lock<std::shared_mutex> lock(GatewayServer::user_Eventloop_mutex_);
        auto it = GatewayServer::user_Eventloop_.find(user_id);
        if(it != GatewayServer::user_Eventloop_.end()) loop = it->second;
    }

    if(loop == nullptr) {
        reactor->Finish(grpc::Status::OK);
        co_return;
    }

    co_await async_SendSingleMsg_for_coro(user_id, message, loop);

    reactor->Finish(grpc::Status::OK);
}

grpc::ServerUnaryReactor* GatewayGrpcServer::sendSingleMsg(grpc::CallbackServerContext* context, 
    const gateway::sendSingleMsgRequest* request, gateway::sendSingleMsgResponse* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    DosendSingleMsg(reactor, request, response);

    return reactor;
}
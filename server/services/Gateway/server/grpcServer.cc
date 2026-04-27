#include "grpcServer.h"
#include <string>

struct SendSingleMsgAwaiter {

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) {

    }

    void await_resume() {}

};

SendSingleMsgAwaiter async_SendSingleMsg_for_coro() {
    return SendSingleMsgAwaiter{};
}

DetachedTask DosendSingleMsg(grpc::ServerUnaryReactor* reactor, const gateway::sendSingleMsgRequest* request, 
    gateway::sendSingleMsgResponse* response) {

    const std::string& message = request->message();

    co_await async_SendSingleMsg_for_coro();

    reactor->Finish(grpc::Status::OK);
}

grpc::ServerUnaryReactor* GatewayGrpcServer::sendSingleMsg(grpc::CallbackServerContext* context, 
    const gateway::sendSingleMsgRequest* request, gateway::sendSingleMsgResponse* response) {

    grpc::ServerUnaryReactor* reactor = context->DefaultReactor();

    DosendSingleMsg(reactor, request, response);

    return reactor;
}
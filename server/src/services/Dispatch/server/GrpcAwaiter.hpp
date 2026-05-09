#pragma once

#include <coroutine>
#include <grpcpp/grpcpp.h>
#include <memory>

#include "muduo/net/EventLoop.h"

template<typename ResponseType, typename RpcInvoker>
struct GrpcAwaiter {

    std::shared_ptr<grpc::ClientContext> context_{std::make_shared<grpc::ClientContext>()};
    std::shared_ptr<ResponseType> response_{std::make_shared<ResponseType>()};
    grpc::Status status_;

    RpcInvoker invoker_;

    GrpcAwaiter(RpcInvoker invoker) : invoker_(std::move(invoker)) {};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        auto loop = muduo::net::EventLoop::getEventLoopOfCurrentThread();

        this->invoker_(this->context_.get(), this->response_.get(), 
        [this, handle, loop] (grpc::Status s) {
            this->status_ = s;

            loop->runInLoop([handle] () {
                handle.resume();
            });
        });
    }

    std::pair<grpc::Status, std::shared_ptr<ResponseType>> await_resume() {
        return {this->status_, this->response_};
    }
};

template<typename ResponseType, typename RpcInvoker>
auto MakeGrpcAwaiter(RpcInvoker invoker) {
    return GrpcAwaiter<ResponseType, RpcInvoker>{std::move(invoker)};
}
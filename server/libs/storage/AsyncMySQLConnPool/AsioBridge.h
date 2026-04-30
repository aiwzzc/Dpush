#pragma once

#include <coroutine>
#include <optional>
#include <exception>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

template<typename T>
struct AsioBridgeAwaiter {

    boost::asio::io_context* ioc_;
    boost::asio::awaitable<T> asio_task_;

    std::optional<T> result_;
    std::exception_ptr ex_;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        boost::asio::co_spawn(*this->ioc_, std::move(this->asio_task_), 
        [handle, this] (std::exception_ptr e, T res) {
            if (e) {
                this->ex_ = e;
            } else {
                this->result_ = std::move(res);
            }

            handle.resume();
        });
    }

    T await_resume() {
        if (ex_) {
            // std::rethrow_exception(ex_);
        }

        return std::move(this->result_.value());
    }
};

template<typename T>
AsioBridgeAwaiter<T> AwaitAsio(boost::asio::io_context* ioc, boost::asio::awaitable<T> task) {
    return AsioBridgeAwaiter<T>{ioc, std::move(task)};
}
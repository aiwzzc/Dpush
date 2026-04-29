#pragma once

#include <memory>
#include <queue>
#include <coroutine>

#include "AsyncMysqlConn.h"

class asyncMysqlConnPool {

public:
    using asyncMysqlConnPtr = std::shared_ptr<asyncMysqlConn>;

    struct AcquireAwaiter {

        asyncMysqlConnPool* pool_;
        asyncMysqlConnPtr result_conn_;

        bool await_ready() const noexcept {
            if(!this->pool_->free_conns_.empty() || 
            this->pool_->active_connections_ < this->pool_->max_connections_) return true;

            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) {
            this->pool_->wait_queue_.push({handle, &this->result_conn_});
        }

        asyncMysqlConnPtr await_resume() {
            if(this->result_conn_) return this->result_conn_;

            if(!this->pool_->free_conns_.empty()) {
                this->result_conn_ = this->pool_->free_conns_.front();
                this->pool_->free_conns_.pop();

                return this->result_conn_;
            }

            this->pool_->active_connections_++;
            return std::make_shared<asyncMysqlConn>();
        }
    };

    AcquireAwaiter Acquire();
    void Release(asyncMysqlConnPtr conn);

private:
    friend AcquireAwaiter;

    int max_connections_;
    int active_connections_;

    std::queue<asyncMysqlConnPtr> free_conns_;

    struct Waiter {
        std::coroutine_handle<> handle_;
        asyncMysqlConnPtr* result_ptr_;
    };

    std::queue<Waiter> wait_queue_;

};
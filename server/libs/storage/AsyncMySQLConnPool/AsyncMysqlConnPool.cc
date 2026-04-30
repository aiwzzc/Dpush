#include "AsyncMysqlConnPool.h"
#include "AsyncMysqlConn.h"

#include <boost/asio/redirect_error.hpp>
#include <boost/system/error_code.hpp>

asyncMysqlConnPool::asyncMysqlConnPool(boost::asio::io_context& ioc, int max_conn)
: ioc_(ioc), max_connections_(max_conn), active_connections_(0) 
{}

asyncMysqlConnPool::~asyncMysqlConnPool() = default;

boost::asio::io_context* asyncMysqlConnPool::get_ioc() const
{ return &this->ioc_; }

boost::asio::awaitable<asyncMysqlConnPool::asyncMysqlConnPtr> asyncMysqlConnPool::Acquire() {
    bool need_create = false;
    asyncMysqlConnPool::asyncMysqlConnPtr conn = nullptr;

    if(!this->free_conns_.empty()) {
        conn = this->free_conns_.front();
        this->free_conns_.pop();

    } else if(this->active_connections_ < this->max_connections_) {
        ++this->active_connections_;
        need_create = true;
    }

    if(conn) co_return conn;

    if(need_create) {
        conn = std::make_shared<asyncMysqlConn>(this);
        co_await conn->async_open();

        co_return conn;
    }

    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::steady_timer timer(executor, boost::asio::steady_timer::time_point::max());

    this->wait_queue_.push({&timer, &conn});

    boost::system::error_code ec;
    co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    co_return conn;
}

void asyncMysqlConnPool::Release(asyncMysqlConnPtr conn) {
    boost::asio::steady_timer* timer_wake;

    if(!this->wait_queue_.empty()) {
        auto& waiter = this->wait_queue_.front();
        this->wait_queue_.pop();

        *(waiter.result_ptr_) = conn;
        timer_wake = waiter.timer_;

    } else {
        this->free_conns_.push(conn);
    }

    if(timer_wake) timer_wake->cancel();
}
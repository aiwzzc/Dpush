#pragma once

#include <memory>
#include <queue>
#include <coroutine>
#include <thread>
#include <mutex>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "AsyncMysqlConn.h"

class asyncMysqlConnPool {

public:
    using asyncMysqlConnPtr = std::shared_ptr<asyncMysqlConn>;

    asyncMysqlConnPool(boost::asio::io_context& ioc, int max_conn);
    ~asyncMysqlConnPool();

    boost::asio::awaitable<asyncMysqlConnPtr> Acquire();
    void Release(asyncMysqlConnPtr conn);
    boost::asio::io_context* get_ioc() const;

private:

    boost::asio::io_context& ioc_;

    int max_connections_;
    int active_connections_;

    std::queue<asyncMysqlConnPtr> free_conns_;

    struct Waiter {
        boost::asio::steady_timer* timer_;
        asyncMysqlConnPtr* result_ptr_;
    };

    std::queue<Waiter> wait_queue_;

};
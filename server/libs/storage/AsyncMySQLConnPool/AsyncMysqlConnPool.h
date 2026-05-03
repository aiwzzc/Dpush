#pragma once

#include <memory>
#include <queue>
#include <coroutine>
#include <thread>
#include <mutex>
#include <vector>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "AsyncMysqlConn.h"

struct mysql_info {

    std::string ip;
    std::string port;
    std::string dbname;
    std::string user;
    std::string password;

};

class asyncMysqlConnPool {

public:
    using asyncMysqlConnPtr = std::shared_ptr<asyncMysqlConn>;

    asyncMysqlConnPool(boost::asio::io_context& ioc, int max_conn, const mysql_info& info);
    ~asyncMysqlConnPool();

    boost::asio::awaitable<asyncMysqlConnPtr> Acquire();
    void Release(asyncMysqlConnPtr conn);
    boost::asio::io_context* get_ioc() const;

private:
    mysql_info meta_info_;

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
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/mysql.hpp>
#include <string>

class asyncMysqlConnPool;
struct mysql_info;

class asyncMysqlConn {

public:
    asyncMysqlConn(asyncMysqlConnPool* pool);

    boost::asio::awaitable<void> async_open(const mysql_info& info);
    boost::asio::awaitable<void> ping();
    boost::asio::awaitable<boost::mysql::results> acquire(const std::string& sql);
    boost::asio::awaitable<boost::mysql::results> execute(const std::string& sql);

private:
    asyncMysqlConnPool* pool_;
    std::shared_ptr<boost::mysql::tcp_connection> conn_;

};

class mysqlConnGuard {

public:
    mysqlConnGuard(asyncMysqlConnPool* pool, std::shared_ptr<asyncMysqlConn> conn);
    ~mysqlConnGuard();

    asyncMysqlConn* operator->();

private:
    std::shared_ptr<asyncMysqlConn> conn_;
    asyncMysqlConnPool* pool_;

};
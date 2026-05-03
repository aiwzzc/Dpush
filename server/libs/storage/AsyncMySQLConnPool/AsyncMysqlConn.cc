#include "AsyncMysqlConn.h"
#include "AsyncMysqlConnPool.h"

namespace mysql = boost::mysql;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

mysqlConnGuard::mysqlConnGuard(asyncMysqlConnPool* pool, std::shared_ptr<asyncMysqlConn> conn) :
pool_(pool), conn_(conn) {}

mysqlConnGuard::~mysqlConnGuard() {
    if(this->conn_ && this->pool_) {
        this->pool_->Release(std::move(this->conn_));
    }
}

asyncMysqlConn* mysqlConnGuard::operator->()
{ return this->conn_.get(); }

asyncMysqlConn::asyncMysqlConn(asyncMysqlConnPool* pool) : 
pool_(pool) {}

boost::asio::awaitable<void> asyncMysqlConn::async_open(const mysql_info& info) {
    auto executor = co_await asio::this_coro::executor;

    this->conn_ = std::make_shared<mysql::tcp_connection>(executor);

    asio::ip::tcp::resolver resolver(executor);
    auto endpoints = co_await resolver.async_resolve(info.ip, info.port, asio::use_awaitable);

    mysql::handshake_params params(info.user, info.password, info.dbname);
   
    co_await this->conn_->async_connect(*endpoints.begin(), params, asio::use_awaitable);
}

boost::asio::awaitable<void> asyncMysqlConn::ping() {

}

boost::asio::awaitable<boost::mysql::results> asyncMysqlConn::acquire(const std::string& sql) {
    mysql::results result;
    mysql::diagnostics diag;

    co_await this->conn_->async_execute(sql.data(), result, diag, asio::use_awaitable);

    co_return result;
}

boost::asio::awaitable<boost::mysql::results> asyncMysqlConn::execute(const std::string& sql) {
    mysql::results result;
    mysql::diagnostics diag;

    co_await this->conn_->async_execute(sql, result, diag, asio::use_awaitable);

    co_return result;
}
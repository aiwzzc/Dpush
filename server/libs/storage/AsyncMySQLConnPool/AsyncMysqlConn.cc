#include "AsyncMysqlConn.h"
#include "AsyncMysqlConnPool.h"

mysqlConnGuard::mysqlConnGuard(asyncMysqlConnPool* pool, std::shared_ptr<asyncMysqlConn> conn) :
pool_(pool), conn_(conn) {}

mysqlConnGuard::~mysqlConnGuard() {
    if(this->conn_ && this->pool_) {
        this->pool_->Release(std::move(this->conn_));
    }
}

asyncMysqlConn* mysqlConnGuard::operator->()
{ return this->conn_.get(); }
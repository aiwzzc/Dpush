#include "SQLOperation.h"
#include "MySQLConn.h"
#include "MySQLConnPool.h"

void SQLOperation::execute(MySQLConn* conn, MySQLConnPool* pool) {
    AsyncResult* res = nullptr;
    if(this->type_ == SQLOperation::SQLType::QUERY) {
        res = new AsyncResult(conn->query(this->sql_), this->cb_);
        res->cb_(res->res_);

    } else if(this->type_ == SQLOperation::SQLType::UPDATE) {
        int ret = conn->update(this->sql_);
        this->cb_({{std::to_string(ret)}});
    }

}

SQLOperation::~SQLOperation() = default;
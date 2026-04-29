#pragma once

#include <boost/mysql.hpp>
#include <memory>

class asyncMysqlConnPool;

class asyncMysqlConn {

public:
    void ping();

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
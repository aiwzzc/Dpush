#pragma once

#include <string>
#include <memory>
#include <future>
#include <vector>
#include <functional>

class MySQLConn;
class MySQLConnPool;

using SQLRow = std::vector<std::string>;
using SQLResult = std::vector<SQLRow>;

class SQLOperation {
public:
    enum class SQLType {
        QUERY,
        UPDATE
    };

    explicit SQLOperation(const std::string& sql, SQLType type, std::function<void(SQLResult)> cb) :
    sql_(sql), type_(type), cb_(std::move(cb)) {}
    ~SQLOperation();

    void execute(MySQLConn* conn, MySQLConnPool* pool);

private:
    std::string sql_;
    SQLType type_;
    std::function<void(SQLResult)> cb_;
};
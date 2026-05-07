#pragma once

#include <string>
#include <vector>
#include <memory>

namespace sql {
    class Driver;
    class Connection;
}

class MySQLWorker;
class SQLOperation;
class MySQLConnPool;
template<typename T>
class BlockQueue;

struct MySQLConnInfo {
    explicit MySQLConnInfo(const std::string& info, const std::string& db);
    ~MySQLConnInfo();

    std::string user;
    std::string password;
    std::string database;
    std::string url;
};

using SQLRow = std::vector<std::string>;
using SQLResult = std::vector<SQLRow>;

class MySQLConn {
public:
    MySQLConn(MySQLConnPool* manager, const std::string& info, const std::string& db, BlockQueue<SQLOperation*>& ask_queue);
    ~MySQLConn();

    SQLResult query(const std::string& sql);
    int update(const std::string& sql);
    int open();
    void close();
    MySQLConnPool* get_manager();

private:
    sql::Driver* driver_;
    std::unique_ptr<sql::Connection> conn_;
    MySQLWorker* worker_;
    MySQLConnPool* manager_;
    MySQLConnInfo info_;
};
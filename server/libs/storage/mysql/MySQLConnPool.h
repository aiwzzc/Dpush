#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <future>
#include <unordered_map>

#include "BlockQueue.h"
#include "SQLOperation.h"

class MySQLConn;
class SQLOperation;

template<typename T>
class BlockQueue;

using SQLRow = std::vector<std::string>;
using SQLResult = std::vector<SQLRow>;

class QueryCallback {
public:
    QueryCallback(std::future<SQLResult>&& future, std::function<void(SQLResult)>&& cb) : 
    future_(std::move(future)), cb_(std::move(cb)) {}

    bool invokeifready() {
        if(this->future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            this->cb_(std::move(this->future_.get()));
            return true;
        }

        return false;
    }

private:

    std::future<SQLResult> future_;
    std::function<void(SQLResult)> cb_;
};

struct AsyncResult {
    explicit AsyncResult(const SQLResult& res, std::function<void(SQLResult)> cb) : res_(res), cb_(cb) {}

    SQLResult res_;
    std::function<void(SQLResult)> cb_;
};

class MySQLConnPool {
public:
    explicit MySQLConnPool(const std::string& db);
    ~MySQLConnPool();

    static MySQLConnPool* getinstance(const std::string& db);
    void query(const std::string& sql, SQLOperation::SQLType type, std::function<void(SQLResult)> cb);
    void initpool(const std::string& url, int pool_size);
    void push_result(AsyncResult* res);
    void process_result();

private:
    
    std::vector<std::unique_ptr<MySQLConn>> pool_;
    std::string database_;
    static std::unordered_map<std::string, MySQLConnPool*> instances_;
    std::unique_ptr<BlockQueue<SQLOperation*>> task_queue_;
    std::unique_ptr<BlockQueue<AsyncResult*>> result_queue_;
};

#include "MySQLConnPool.h"
#include "BlockQueue.h"
#include "MySQLConn.h"

std::unordered_map<std::string, MySQLConnPool*> MySQLConnPool::instances_;

MySQLConnPool* MySQLConnPool::getinstance(const std::string& db) {
    if(instances_.find(db) == instances_.end()) instances_[db] = new MySQLConnPool(db);

    return instances_[db];
}

void MySQLConnPool::initpool(const std::string& url, int pool_size) {
    this->task_queue_ = std::make_unique<BlockQueue<SQLOperation*>>();
    this->result_queue_ = std::make_unique<BlockQueue<AsyncResult*>>();

    for(int i = 0; i < pool_size; ++i) {
        MySQLConn* conn = new MySQLConn(this, url, this->database_, *this->task_queue_);
        conn->open();
        this->pool_.push_back(std::unique_ptr<MySQLConn>(conn));
    }
}

void MySQLConnPool::query(const std::string& sql, SQLOperation::SQLType type, std::function<void(SQLResult)> cb) {
    SQLOperation* op = new SQLOperation(sql, type, cb);

    this->task_queue_->push_back(op);
}

void MySQLConnPool::process_result() {

    AsyncResult* res = nullptr;

    while(this->result_queue_->try_pop(res)) {

        if(res) {
            res->cb_(res->res_);

            delete res;
        }
    }
}

void MySQLConnPool::push_result(AsyncResult* res) { this->result_queue_->push_back(res); }

MySQLConnPool::MySQLConnPool(const std::string& db) : database_(db) {}

MySQLConnPool::~MySQLConnPool() {
    this->task_queue_->cancel();
    this->result_queue_->cancel();
}
#include "MySQLWorker.h"
#include "BlockQueue.h"
#include "SQLOperation.h"
#include "MySQLConn.h"

void MySQLWorker::start() {
    this->worker_ = std::thread(&MySQLWorker::run, this);
}

void MySQLWorker::stop() {
    if(this->worker_.joinable()) this->worker_.join();
}

MySQLWorker::~MySQLWorker() { this->stop(); }

void MySQLWorker::run() {
    while(1) {
        SQLOperation* op = nullptr;

        if(this->task_queue_.pop_front(op) == false) break;
        op->execute(this->conn_, this->conn_->get_manager());

        delete op;
    }

}
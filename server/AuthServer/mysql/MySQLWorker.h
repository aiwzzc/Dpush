#pragma once

#include <thread>

class MySQLConn;
class SQLOperation;

template<typename T>
class BlockQueue;

class MySQLWorker {
public:
    MySQLWorker(MySQLConn* conn, BlockQueue<SQLOperation*>& task_queue) : conn_(conn), task_queue_(task_queue) {}
    ~MySQLWorker();

    void start();
    void stop();

private:
    void run();
    std::thread worker_;
    MySQLConn* conn_;
    BlockQueue<SQLOperation*>& task_queue_;

};
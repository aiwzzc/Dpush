#pragma once

#include <memory>
#include <vector>
#include <thread>
#include <atomic>

#include "AsyncMysqlConnPool.h"

class asyncMysqlCluster {

public:
    asyncMysqlCluster(int thread_count, int pool_size_per_thread);
    ~asyncMysqlCluster();

    void start();
    asyncMysqlConnPool* get_next_pool();

private:
    std::vector<std::unique_ptr<boost::asio::io_context>> iocs_;
    std::vector<std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>> worker_guards_;
    std::vector<std::unique_ptr<asyncMysqlConnPool>> pools_;
    std::vector<std::thread> workers_;

    std::atomic<int> next_idx_;

};
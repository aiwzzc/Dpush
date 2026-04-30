#include "AsyncMysqlCluster.h"

asyncMysqlCluster::asyncMysqlCluster(int thread_count, int pool_size_per_thread) {
    for(int i = 0; i < thread_count; ++i) {
        auto ioc = std::make_unique<boost::asio::io_context>();
        auto work_guard = 
        std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(ioc->get_executor());

        auto pool = std::make_unique<asyncMysqlConnPool>(*ioc, pool_size_per_thread);

        this->iocs_.emplace_back(std::move(ioc));
        this->worker_guards_.emplace_back(std::move(work_guard));
        this->pools_.emplace_back(std::move(pool));
    }
}

asyncMysqlCluster::~asyncMysqlCluster() {
    for (auto& ioc : iocs_) ioc->stop();
    for (auto& t : this->workers_) {
        if (t.joinable()) t.join();
    }
}

void asyncMysqlCluster::start() {
    for(auto& ioc : this->iocs_) {
        this->workers_.emplace_back([&ioc] () {
            ioc->run();
        });
    }
}

asyncMysqlConnPool* asyncMysqlCluster::get_next_pool() {
    auto index = this->next_idx_.fetch_add(1, std::memory_order_relaxed) % this->pools_.size();
    return this->pools_[index].get();
}
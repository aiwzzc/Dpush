#include "AsyncMySQLConnPool/AsyncMysqlConn.h"
#include "AsyncMySQLConnPool/AsioBridge.h"
#include "AsyncMySQLConnPool/AsyncMysqlConnPool.h"
#include "AsyncMySQLConnPool/AsyncMysqlCluster.h"

#include "mysql/BlockQueue.h"
#include "mysql/MySQLConn.h"
#include "mysql/MySQLConnPool.h"
#include "mysql/MySQLWorker.h"
#include "mysql/SQLOperation.h"

#include "coroutineTask.h"

#include <random>
#include <string>
#include <coroutine>
#include <iostream>
#include <atomic>
#include <latch>
#include <memory>

constexpr const char* dbname            = "chatroom";
constexpr const char* url               = "tcp://127.0.0.1:3306;root;zzc1109aiw";

constexpr int total_queries = 100000;
constexpr int concurrency = 100;  // 并发启动 50 个协程同时去抢连接
constexpr int pool_size = 6; 
constexpr int queries_per_worker = total_queries / concurrency;
constexpr int thread_count = 6;
constexpr int pool_size_per_thread = 10; // 每个线程 5 个连接 (总共 20 个连接)

std::vector<std::string> sql_pool;
std::vector<std::vector<std::string>> async_sql_pool(concurrency, std::vector<std::string>(queries_per_worker));

void init_sql_pool(int N)
{
    sql_pool.reserve(N);

    for (int i = 1; i <= N; ++i)
    {

        static thread_local std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<int> dist(1, 700000);

        int id = dist(rng);

        sql_pool.emplace_back(
            "SELECT * FROM users WHERE username = 'user_" 
            + std::to_string(id) + "'"
        );
    }
}

void init_async_sql_pool() {

    for (int i = 0; i < concurrency; ++i) {
        std::vector<std::string> sqls_(queries_per_worker);
        for(int j = 0; j < queries_per_worker; ++j) {
            static thread_local std::mt19937 rng(std::random_device{}());
            static std::uniform_int_distribution<int> dist(1, 700000);

            int id = dist(rng);

            sqls_[j] = "SELECT * FROM users WHERE username = 'user_" 
            + std::to_string(id) + "'";
        }

        async_sql_pool[i] = std::move(sqls_);
    }
}

std::string gen_sql() {
    static thread_local std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<int> dist(1, 700000);

    int id = dist(rng);

    return "SELECT * FROM users WHERE username = 'user_" 
           + std::to_string(id) + "'";
}

void fake_async_mysql() {
    MySQLConnPool* pool = MySQLConnPool::getinstance(dbname);
    pool->initpool(url, thread_count);
    std::atomic<int> done{0};

    init_sql_pool(total_queries);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total_queries; i++) {
        // auto sql = gen_sql();

        pool->query(sql_pool[i], SQLOperation::SQLType::QUERY, [&done] (SQLResult res) {
            ++done;
        });
    }

    while(done < total_queries) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << cost.count() << " ms\n";
}

DetachedTask worker_coro(asyncMysqlConnPool* pool, int queries_to_do, 
    std::shared_ptr<std::latch> latch, int index) {
    auto& sqls = async_sql_pool[index];

    for (int i = 0; i < queries_to_do; ++i) {
        try {
            // auto sql = gen_sql();
        
            auto conn = co_await AwaitAsio(pool->get_ioc(), pool->Acquire());
            mysqlConnGuard guard(pool, conn);
            
            co_await AwaitAsio(pool->get_ioc(), guard->execute(sqls[i]));
            
        } catch (const std::exception& e) {
            std::cerr << "Worker error: " << e.what() << std::endl;
        }

        latch->count_down();
    }
    
}

boost::asio::awaitable<void> pure_asio_worker(asyncMysqlConnPool* pool, int queries_to_do, 
    std::shared_ptr<std::latch> latch) {
    for (int i = 0; i < queries_to_do; ++i) {
        try {
            auto sql = gen_sql();
            
            // 直接 co_await 获取连接
            auto conn = co_await pool->Acquire();
            mysqlConnGuard guard(pool, conn);
            
            // 直接执行
            co_await guard->execute(sql);
            
        } catch (const std::exception& e) {
            std::cerr << "Query Failed: " << e.what() << std::endl;
        }
        
        latch->count_down();
    }
}

void true_async_mysql() {

    asyncMysqlCluster cluster(thread_count, pool_size_per_thread);
    cluster.start();

    init_async_sql_pool();
    asyncMysqlConnPool* pool = cluster.get_next_pool();

    auto latch = std::make_shared<std::latch>(total_queries);
    auto start = std::chrono::high_resolution_clock::now();

    for(int i = 0; i < concurrency; ++i) {
        asyncMysqlConnPool* target_pool = cluster.get_next_pool();

        worker_coro(target_pool, queries_per_worker, latch, i);
    }

    latch->wait();

    auto end = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << cost.count() << " ms\n";
}

int main(int argc, char* argv[]) {

    if(argc != 2) return -1;

    if(strcmp(argv[1], "1") == 0) {
        fake_async_mysql();

    } else if(strcmp(argv[1], "2") == 0) {
        true_async_mysql();
    }

    return 0;
}
#include "AsyncMysqlConnPool.h"

asyncMysqlConnPool::AcquireAwaiter asyncMysqlConnPool::Acquire() 
{ return AcquireAwaiter{this}; }

void asyncMysqlConnPool::Release(asyncMysqlConnPtr conn) {
    if(!this->wait_queue_.empty()) {
        Waiter& waiter = this->wait_queue_.front();
        this->wait_queue_.pop();

        *(waiter.result_ptr_) = conn;
        waiter.handle_.resume();
        
    } else {
        this->free_conns_.push(conn);
    }
}
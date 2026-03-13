#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class BlockQueue {
public:
    explicit BlockQueue(bool is_block = true) : is_block_(is_block) {}
    ~BlockQueue() = default;

    void push_back(const T& task) {
        std::lock_guard<std::mutex> lock(this->produce_mu_);
        this->produce_tasks_.push(task);
        cond.notify_one();
    }

    bool pop_front(T& task) {
        std::lock_guard<std::mutex> lock(this->consum_mu_);
        if(this->consum_tasks_.empty() && swap_queue() == 0) return false;

        task = std::move(this->consum_tasks_.front());
        this->consum_tasks_.pop();

        return true;
    }

    bool try_pop(T& task) {
        std::lock_guard<std::mutex> lock(this->consum_mu_);

        if(!this->consum_tasks_.empty()) {
            task = std::move(this->consum_tasks_.front());
            this->consum_tasks_.pop();

            return true;
        }

        {
            std::unique_lock<std::mutex> lock(this->produce_mu_);

            if(this->produce_tasks_.empty()) return false;

            swap(this->consum_tasks_, this->produce_tasks_);
        }

        task = std::move(this->consum_tasks_.front());
        this->consum_tasks_.pop();

        return true;
    }

    void cancel() {
        std::lock_guard<std::mutex> lock(this->produce_mu_);
        this->is_block_ = false;

        cond.notify_all();
    }

private:

    int swap_queue() {
        {
            std::unique_lock<std::mutex> lock(this->produce_mu_);
            cond.wait(lock, [this] { return !this->is_block_ || !this->produce_tasks_.empty(); });

            swap(this->consum_tasks_, this->produce_tasks_);
            return this->consum_tasks_.size();
        }
    }

    bool is_block_;
    std::condition_variable cond;

    std::mutex consum_mu_;
    std::mutex produce_mu_;

    std::queue<T> consum_tasks_;
    std::queue<T> produce_tasks_;
};
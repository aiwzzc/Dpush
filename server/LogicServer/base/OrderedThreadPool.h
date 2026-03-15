#pragma once

#include <thread>
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <future>
#include <memory>
#include <cstring>

class OrderedthreadTask {

public:
    OrderedthreadTask() noexcept : call_(nullptr), destore_(nullptr) {}

    template <typename F>
    OrderedthreadTask(F&& f) {
        using Fn = std::decay_t<F>;
        static_assert(sizeof(Fn) <= StorageSize, "Task too large");

        new (&this->storage_) Fn(std::forward<F>(f));

        this->call_ = [] (void* p) {
            (*reinterpret_cast<Fn*>(p))();
        };

        this->destore_ = [] (void* p) {
            reinterpret_cast<Fn*>(p)->~Fn();
        };
    }

    OrderedthreadTask(const OrderedthreadTask&) = delete;
    OrderedthreadTask& operator=(const OrderedthreadTask&) = delete;

    OrderedthreadTask(OrderedthreadTask&& other) noexcept {
        moveFrom(std::move(other));
    }

    OrderedthreadTask& operator=(OrderedthreadTask&& other) noexcept {
        if(this != &other) {
            reset();
            moveFrom(std::move(other));
        }

        return *this;
    }

    ~OrderedthreadTask() { reset(); }

    void operator()() { call_(&this->storage_); }

private:
    static constexpr std::size_t StorageSize = 64;
    alignas(std::max_align_t) unsigned char storage_[StorageSize];

    void (*call_)(void*);
    void (*destore_)(void*);

    void reset() {
        if(this->destore_) {
            this->destore_(&this->storage_);
            this->call_ = nullptr;
            this->destore_ = nullptr;
        }
    }

    void moveFrom(OrderedthreadTask&& other) {
        std::memcpy(&this->storage_, &other.storage_, this->StorageSize);
        this->call_ = other.call_;
        this->destore_ = other.destore_;

        other.call_ = nullptr;
        other.destore_ = nullptr;
    }
};

template<typename T>
class Orderedblockqueue {

public:
    Orderedblockqueue() { this->isblocking_.store(true); }
    ~Orderedblockqueue() {}

    bool pushback(T task) {
        std::lock_guard<std::mutex> lock(this->produceMutex_);
        if(this->isblocking_.load() == false) return false;

        this->producer_.push(std::move(task));
        this->cond_.notify_one();

        return true;
    }

    bool popfront(T& task) {
        if(this->consumer_.empty() && this->SwapQueue() == false) return false;

        task = std::move(this->consumer_.front());
        this->consumer_.pop();

        return true;
    }

    void cancel() {
        std::lock_guard<std::mutex> lock(this->produceMutex_);
        this->isblocking_.store(false);
        this->cond_.notify_all();
    }

private:
    bool SwapQueue() {
        std::unique_lock<std::mutex> lock(this->produceMutex_);
        this->cond_.wait(lock, [this] { return !this->producer_.empty() || this->isblocking_.load() == false; });

        if(this->producer_.empty() && this->isblocking_.load() == false) {
            return false;
        }

        std::swap(this->consumer_, this->producer_);
        return !this->consumer_.empty();
    }

    std::queue<T> consumer_;
    std::queue<T> producer_;

    std::mutex produceMutex_;

    std::condition_variable cond_;
    std::atomic<bool> isblocking_;
};

class OrderedThreadPool {

public:
    OrderedThreadPool(std::size_t threadnum = 1) : threadnum_(threadnum), tasks_list_(threadnum) {}
    ~OrderedThreadPool() {
        for(auto& tasks : this->tasks_list_) {
            tasks.cancel();
        }

        for(auto& worker : this->worker_) {
            if(worker.joinable()) worker.join();
        }
    }

    void start() {
        for(std::size_t i = 0; i < this->threadnum_; ++i) {
            this->worker_.emplace_back([this, i] { this->workerLoop(i); });
        }
    }

    template<typename Index, typename F, typename... Args>
    auto submit(Index&& key, F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using ResType = std::invoke_result_t<F, Args...>;
        using IndexType = std::decay_t<Index>;

        auto taskPtr = std::make_shared<std::packaged_task<ResType()>>(
            [func = std::forward<F>(f), ... params = std::forward<Args>(args)] () mutable -> ResType {
                return std::invoke(func, params...);
            }
        );

        std::future<ResType> future = taskPtr->get_future();

        std::size_t index = std::hash<IndexType>{}(key) % this->threadnum_;

        this->tasks_list_[index].pushback(OrderedthreadTask{
            [taskPtr] {
                (*taskPtr)();
            }
        });

        return future;
    }

private:
    void workerLoop(std::size_t queue_index) {
        while(1) {
            OrderedthreadTask task;

            if(this->tasks_list_[queue_index].popfront(task) == false) break;

            try {
                task();

            } catch(...) {}
        }
    }

    std::size_t threadnum_;
    std::vector<Orderedblockqueue<OrderedthreadTask>> tasks_list_;
    std::vector<std::thread> worker_;
};
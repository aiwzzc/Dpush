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

class threadTask {

public:
    threadTask() noexcept : call_(nullptr), destore_(nullptr) {}

    template <typename F>
    threadTask(F&& f) {
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

    threadTask(const threadTask&) = delete;
    threadTask& operator=(const threadTask&) = delete;

    threadTask(threadTask&& other) noexcept {
        moveFrom(std::move(other));
    }

    threadTask& operator=(threadTask&& other) noexcept {
        if(this != &other) {
            reset();
            moveFrom(std::move(other));
        }

        return *this;
    }

    ~threadTask() { reset(); }

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

    void moveFrom(threadTask&& other) {
        std::memcpy(&this->storage_, &other.storage_, this->StorageSize);
        this->call_ = other.call_;
        this->destore_ = other.destore_;

        other.call_ = nullptr;
        other.destore_ = nullptr;
    }
};

template<typename T>
class blockqueue {

public:
    blockqueue() { this->isblocking_.store(true); }
    ~blockqueue() {}

    bool pushback(T&& task) {
        std::lock_guard<std::mutex> lock(this->produceMutex_);
        if(this->isblocking_.load() == false) return false;

        this->producer_.push(std::move(task));
        this->cond_.notify_one();

        return true;
    }

    bool popfront(T& task) {
        std::lock_guard<std::mutex> lock(this->consumMutex_);
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

    std::mutex consumMutex_;
    std::mutex produceMutex_;

    std::condition_variable cond_;
    std::atomic<bool> isblocking_;
};

class ComputeThreadPool {

public:
    ComputeThreadPool(std::size_t threadnum = 1) : threadnum_(threadnum) {}
    ~ComputeThreadPool() {
        this->tasks_.cancel();

        for(auto& worker : this->worker_) {
            if(worker.joinable()) worker.join();
        }
    }

    void start() {
        for(int i = 0; i < this->threadnum_; ++i) {
            this->worker_.emplace_back([this] { workerLoop(); });
        }
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using ResType = std::invoke_result_t<F, Args...>;

        auto taskPtr = std::make_shared<std::packaged_task<ResType()>>(
            [func = std::forward<F>(f), ... params = std::forward<Args>(args)] () mutable -> ResType {
                return std::invoke(func, params...);
            }
        );

        std::future<ResType> future = taskPtr->get_future();
        this->tasks_.pushback(threadTask{
            [taskPtr] {
                (*taskPtr)();
            }
        });

        return future;
    }

private:
    void workerLoop() {
        while(1) {
            threadTask task;

            if(this->tasks_.popfront(task) == false) break;

            try {
                task();

            } catch(...) {}
        }
    }

    std::size_t threadnum_;
    blockqueue<threadTask> tasks_;
    std::vector<std::thread> worker_;
};
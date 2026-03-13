#ifndef API_COMMON_TASK_H
#define API_COMMON_TASK_H

#include <coroutine>
#include <exception>
#include <utility>

// 内部基础 Promise 类，用于处理协程的挂起和恢复链
namespace detail {
    struct TaskPromiseBase {
        std::coroutine_handle<> continuation_; // 记录是谁 co_await 了我

        // 协程启动时立即挂起（这是标准做法）
        std::suspend_always initial_suspend() noexcept { return {}; }

        // 协程结束时，唤醒等待它的上一层协程（对称转移机制）
        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            template <typename PromiseType>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> h) noexcept {
                if (h.promise().continuation_) {
                    return h.promise().continuation_; // 恢复父协程
                }
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }
        void unhandled_exception() { std::terminate(); }
    };
}

// ================= 主模板 Task<T> =================
template<typename T>
struct [[nodiscard]] Task {
    struct promise_type : detail::TaskPromiseBase {
        T value_;
        Task get_return_object() noexcept {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        void return_value(T value) noexcept { value_ = value; }
    };

    std::coroutine_handle<promise_type> handle_;

    explicit Task(std::coroutine_handle<promise_type> h) noexcept : handle_(h) {}
    Task(Task&& t) noexcept : handle_(std::exchange(t.handle_, {})) {}
    ~Task() { if (handle_) handle_.destroy(); }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // 让 Task 可以被 co_await
    bool await_ready() const noexcept { return !handle_ || handle_.done(); }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept {
        handle_.promise().continuation_ = awaiting_coroutine;
        return handle_; // 转移执行权
    }
    T await_resume() {
        return handle_.promise().value_; // 返回结果
    }
};

// ================= 特化模板 Task<void> =================
template<>
struct [[nodiscard]] Task<void> {
    struct promise_type : detail::TaskPromiseBase {
        Task get_return_object() noexcept {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        void return_void() noexcept {} // void 特化必须使用 return_void
    };

    std::coroutine_handle<promise_type> handle_;

    explicit Task(std::coroutine_handle<promise_type> h) noexcept : handle_(h) {}
    Task(Task&& t) noexcept : handle_(std::exchange(t.handle_, {})) {}
    ~Task() { if (handle_) handle_.destroy(); }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // 让 Task<void> 可以被 co_await
    bool await_ready() const noexcept { return !handle_ || handle_.done(); }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept {
        handle_.promise().continuation_ = awaiting_coroutine;
        return handle_; 
    }
    void await_resume() {} // 返回 void
};

// ================= 根协程专用的发射后不管任务 =================
struct DetachedTask {
    struct promise_type {
        DetachedTask get_return_object() noexcept { return {}; }
        
        // 【关键魔法 1】：变成 Eager 模式！协程一创建，立刻往下执行！
        std::suspend_never initial_suspend() noexcept { return {}; }
        
        // 【关键魔法 2】：协程执行完后，不需要别人来拿结果，立刻自动清理内存销毁！
        std::suspend_never final_suspend() noexcept { return {}; }
        
        void return_void() noexcept {}
        
        void unhandled_exception() {
            // 这里可以加打印日志，防止协程内抛出异常导致崩溃
            std::terminate(); 
        }
    };
};

#endif // API_COMMON_TASK_H
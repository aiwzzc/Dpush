#pragma once

#include <atomic>
#include <optional>

namespace pulse::Logger {

#ifdef __cpp_lib_hardware_interference_size
#include <new>
    constexpr std::size_t KSCacheLineSize = std::hardware_destructive_interference_size;
#else
    constexpr std::size_t KSCacheLineSize = 64;
#endif

// 实际容量为 Capacity - 1
template<typename T, std::size_t Capacity>
class SpscRingBuffer {

public:
    static_assert(Capacity && !(Capacity & (Capacity - 1)), "Capacity must be power of 2");
    static_assert(std::is_nothrow_move_constructible_v<T>, "T must be nothrow move constructible");

    static constexpr std::size_t KMask = Capacity - 1;

    SpscRingBuffer() = default;
    ~SpscRingBuffer() {
        std::size_t read = this->consumer_.r_idx_.load(std::memory_order_relaxed);
        std::size_t write = this->producer_.w_idx_.load(std::memory_order_relaxed);

        while(read != write) {
            Slot& slot = this->buffer_[read & KMask];
            T* data = std::launder(reinterpret_cast<T*>(&slot.storage));
            data->~T();
            read = (read + 1) & (Capacity - 1);
        }
    }

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    SpscRingBuffer(SpscRingBuffer&&) = delete;
    SpscRingBuffer& operator=(SpscRingBuffer&&) = delete;

    template<typename... Args>
    bool enqueue(Args&&... args) {
        const std::size_t write = this->producer_.w_idx_.load(std::memory_order_relaxed);
        const std::size_t write_next = (write + 1) & (Capacity - 1);

        if(write_next == this->producer_.r_idx_cache_) {
            this->producer_.r_idx_cache_ = this->consumer_.r_idx_.load(std::memory_order_acquire);
            if(write_next == this->producer_.r_idx_cache_) return false;
        }

        new (&this->buffer_[write]) T(std::forward<Args>(args)...);
        this->producer_.w_idx_.store(write_next, std::memory_order_release);

        return true;
    }
    
    std::optional<T> dequeue() {
        const std::size_t read = this->consumer_.r_idx_.load(std::memory_order_relaxed);

        if(read == this->consumer_.w_idx_cache_) {
            this->consumer_.w_idx_cache_ = this->producer_.w_idx_.load(std::memory_order_acquire);
            if(read == this->consumer_.w_idx_cache_) return std::nullopt;
        }

        T* data = std::launder(reinterpret_cast<T*>(&this->buffer_[read]));
        std::optional<T> result(std::in_place, std::move(*data));
        data->~T();

        this->consumer_.r_idx_.store((read + 1) & (Capacity - 1), std::memory_order_release);

        return result;
    }

    // 两次load之间状态可能发生变化;也就是size()为近似值;并不是当前快照
    std::size_t size_approx() const noexcept {
        const std::size_t read = this->consumer_.r_idx_.load(std::memory_order_acquire);
        const std::size_t write = this->producer_.w_idx_.load(std::memory_order_acquire);

        return write >= read ? write - read : (Capacity - read + write);
    }

    bool empty_approx() const noexcept {
        return this->size_approx() == 0;
    }

    static constexpr std::size_t capacity() noexcept { 
        return Capacity;
    }

private:
    struct ProducerState {
        std::size_t r_idx_cache_{0};
        std::atomic<std::size_t> w_idx_{0};
    };

    struct ConsumerState {
        std::size_t w_idx_cache_{0};
        std::atomic<std::size_t> r_idx_{0};
    };

    alignas(KSCacheLineSize) ProducerState producer_;
    char pad_producer_[KSCacheLineSize - sizeof(ProducerState)]{};

    alignas(KSCacheLineSize) ConsumerState consumer_;
    char pad_consumer_[KSCacheLineSize - sizeof(ConsumerState)]{};

    struct Slot {
        alignas(T) std::byte storage[sizeof(T)];
    };

    Slot buffer_[Capacity];
    char pad_end_[KSCacheLineSize]{};

};

};
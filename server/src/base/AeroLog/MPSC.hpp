#pragma once

#include <atomic>
#include <optional>

namespace pulse::Logger {

#ifdef __cpp_lib_hardware_interference_size
#include <new>
    constexpr std::size_t KCacheLineSize = std::hardware_destructive_interference_size;
#else
    constexpr std::size_t KCacheLineSize = 64;
#endif

template<typename T, std::size_t Capacity>
class MpscRingBuffer {

public:
    static_assert(Capacity && !(Capacity & (Capacity - 1)), "Capacity must be power of 2");
    static_assert(std::is_nothrow_move_constructible_v<T>, "T must be nothrow move constructible");

    static constexpr std::size_t KMask = Capacity - 1;

    MpscRingBuffer(const MpscRingBuffer&) = delete;
    MpscRingBuffer& operator=(const MpscRingBuffer&) = delete;

    MpscRingBuffer(MpscRingBuffer&&) = delete;
    MpscRingBuffer& operator=(MpscRingBuffer&&) = delete;

    MpscRingBuffer() {
        for(int i = 0; i < Capacity; ++i) {
            this->buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~MpscRingBuffer() {
        std::size_t head = this->head_.load(std::memory_order_relaxed);
        std::size_t tail = this->tail_.load(std::memory_order_relaxed);

        while(head != tail) {
            Slot& slot = this->buffer_[head & KMask];
            T* data = std::launder(reinterpret_cast<T*>(&slot.storage));
            data->~T();
            ++head;
        }
    }

    template<typename... Args>
    bool enqueue(Args&&... args) {
        static_assert(std::is_nothrow_constructible_v<T, Args...>, "T construction must not throw");

        std::size_t tail = this->tail_.load(std::memory_order_relaxed);

        while(true) {
            Slot& slot = this->buffer_[tail & KMask];
            std::size_t seq = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(tail);

            int spin{0};

            if(diff == 0) {
                if(this->tail_.compare_exchange_weak(tail, tail + 1, std::memory_order_relaxed)) {
                    new (&slot.storage) T(std::forward<Args>(args)...);
                    slot.sequence.store(tail + 1, std::memory_order_release);

                    return true;
                }

                ++spin;
                if(spin > 16) {
#if defined(__x86_64__)
                    __builtin_ia32_pause();
#elif defined(__aarch64__)
                    __asm__ __volatile__("yield");
#endif
                    spin = 0;
                }
            }

            else if(diff < 0) {
                return false;
            }

            else {
                tail = this->tail_.load(std::memory_order_relaxed);
            }
        }
    }

    std::optional<T> dequeue() {
        std::size_t head = this->head_.load(std::memory_order_relaxed);
        Slot& slot = this->buffer_[head & KMask];
        std::size_t seq = slot.sequence.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(head + 1);

        if(diff != 0) return std::nullopt;

        T* data = std::launder(reinterpret_cast<T*>(&slot.storage));
        std::optional<T> result(std::in_place, std::move(*data));
        data->~T();

        slot.sequence.store(head + Capacity, std::memory_order_release);
        this->head_.store(head + 1, std::memory_order_relaxed);

        return result;
    }

    std::size_t size_approx() const noexcept {
        const std::size_t read = this->head_.load(std::memory_order_acquire);
        const std::size_t write = this->tail_.load(std::memory_order_acquire);

        return write - read;
    }

    bool empty_approx() const noexcept {
        return this->size_approx() == 0;
    }

    static constexpr std::size_t capacity() noexcept { 
        return Capacity;
    }

private:
    alignas(KCacheLineSize) std::atomic<std::size_t> tail_{0};
    alignas(KCacheLineSize) std::atomic<std::size_t> head_{0};

    struct Slot {
        std::atomic<std::size_t> sequence{0};
        alignas(T) std::byte storage[sizeof(T)];
    };

    alignas(KCacheLineSize) Slot buffer_[Capacity];
    char pad_end_[KCacheLineSize]{};

};

};
#pragma once

#include "MPSC.hpp"
#include <benchmark/benchmark.h>
#include <thread>
#include <vector>

namespace mybenchmark::mpsc {

constexpr std::size_t Capacity = 1024;

class verify_mpsc {

private:
    struct Msg {
        int pid_;
        int seq_;

        Msg(int pid, int seq) noexcept  : 
        pid_(pid), seq_(seq) {};
    };

public:
    void verify(int n_producer, int pre_producer) {
        std::vector<std::thread> producers;
        for(int pid = 0; pid < n_producer; ++pid) {
            producers.emplace_back([this, pid, pre_producer] () {
                for(int i = 0; i < pre_producer; ++i) {
                    while(!this->mpsc_.enqueue(pid, i)) {
                        std::this_thread::yield();
                    }
                }
            });
        }

        std::thread consumer([this, n_producer, pre_producer] () {
            int total = n_producer * pre_producer;
            int received{0};

            std::vector<int> last(n_producer, -1);

            while(received < total) {
                auto msg = this->mpsc_.dequeue();
                if(!msg) {
                    std::this_thread::yield();
                    continue;
                }

                Msg m = msg.value();

                if(m.pid_ < 0 || m.pid_ >= n_producer) {
                    fprintf(stderr, "污染:pid 越界 %d\n", m.pid_);
                    std::abort();

                } else if(m.seq_ < 0 || m.seq_ >= pre_producer) {
                    fprintf(stderr, "污染:pid=%d seq 越界 %d\n", m.pid_, m.seq_);
                    std::abort();
                }

                int expected = last[m.pid_] + 1;
                if(m.seq_ != expected) {
                    fprintf(stderr,
                    "FIFO 违反:pid=%d expected seq=%d, got seq=%d "
                    "(丢失/重复/乱序)\n",
                    m.pid_, expected, m.seq_);
                    std::abort();
                }

                last[m.pid_] = m.seq_;
                ++received;
            }

            for(int i = 0; i < n_producer; ++i) {
                if(last[i] != pre_producer - 1) {
                    fprintf(stderr,
                    "生产者 %d 未收齐: last=%d, expected=%d\n",
                    i, last[i], pre_producer - 1);
                    std::abort();
                }

                printf("验证通过:%d 条消息全部按序收到\n", total);
            }
        });

        for(auto& thread : producers) thread.join();
        consumer.join(); 
    }

private:
    pulse::Logger::MpscRingBuffer<Msg, Capacity> mpsc_;

};

template<typename T>
class mpsc_ben {

public:
    void producer() {

    }

    void consumer() {

    }

private:
    pulse::Logger::MpscRingBuffer<T, Capacity> mpsc_;

};

};

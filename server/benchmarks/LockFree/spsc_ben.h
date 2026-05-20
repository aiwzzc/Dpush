#pragma once

#include "SPSC.hpp"
#include <benchmark/benchmark.h>

namespace mybenchmark::spsc {

constexpr std::size_t Capacity = 1024;

template<typename T>
class spsc_bench {

public:
    void producer() {
        
    }

    void consumer() {

    }

private:
    pulse::Logger::SpscRingBuffer<T, Capacity> spsc_;

};

};
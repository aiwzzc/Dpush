#pragma once

#include <iostream>
#include <chrono>
#include <mutex>
#include <stdexcept>

class SnowflakeIdWorker {
public:
    // worker_id 是当前机器的 ID (0 ~ 1023)
    // 如果你有多个逻辑服，给每个逻辑服分配一个不同的 ID
    SnowflakeIdWorker(int64_t worker_id = 1) : workerId_(worker_id) {
        if (worker_id > maxWorkerId_ || worker_id < 0) {
            throw std::invalid_argument("worker Id can't be greater than 1023 or less than 0");
        }
    }

    // 核心生成方法
    int64_t nextId() {
        std::lock_guard<std::mutex> lock(mutex_);

        int64_t timestamp = timeGen();

        // 发生了时钟回拨
        if (timestamp < lastTimestamp_) {
            throw std::runtime_error("Clock moved backwards. Refusing to generate id");
        }

        // 同一毫秒内生成
        if (lastTimestamp_ == timestamp) {
            sequence_ = (sequence_ + 1) & sequenceMask_;
            // 毫秒内序列溢出，等待下一毫秒
            if (sequence_ == 0) {
                timestamp = tilNextMillis(lastTimestamp_);
            }
        } else {
            // 时间戳改变，毫秒内序列重置
            sequence_ = 0;
        }

        lastTimestamp_ = timestamp;

        // 移位并拼接在一起组成 64 位 ID
        return ((timestamp - twepoch_) << timestampLeftShift_)
             | (workerId_ << workerIdShift_)
             | sequence_;
    }

    static SnowflakeIdWorker& getInstance() {
        static SnowflakeIdWorker instance;
        return instance;
    }

private:
    int64_t timeGen() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    int64_t tilNextMillis(int64_t lastTimestamp) const {
        int64_t timestamp = timeGen();
        while (timestamp <= lastTimestamp) {
            timestamp = timeGen();
        }
        return timestamp;
    }

    std::mutex mutex_;
    
    // 初始纪元时间戳 (比如 2024-01-01 00:00:00 的毫秒数)
    // 设定的越近，可用的年份越长
    const int64_t twepoch_ = 1704067200000L; 

    const int64_t workerIdBits_ = 10L;
    const int64_t maxWorkerId_ = -1L ^ (-1L << workerIdBits_); // 1023
    const int64_t sequenceBits_ = 12L;

    const int64_t workerIdShift_ = sequenceBits_;              // 12
    const int64_t timestampLeftShift_ = sequenceBits_ + workerIdBits_; // 22
    const int64_t sequenceMask_ = -1L ^ (-1L << sequenceBits_); // 4095

    int64_t workerId_;
    int64_t sequence_ = 0L;
    int64_t lastTimestamp_ = -1L;
};
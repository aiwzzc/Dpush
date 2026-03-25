#pragma once

#include <liburing.h>
#include <cstring>
#include <sys/eventfd.h>
#include <vector>
#include <string>
#include <memory>
#include <fcntl.h>
#include <unistd.h>

#include "muduo/net/EventLoop.h"
#include "muduo/net/Channel.h"
#include "muduo/base/Timestamp.h"

using muduo::net::EventLoop;
using muduo::net::Channel;
using muduo::Timestamp;

#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

constexpr size_t kEntresLength = 65536;

struct list_head {
    list_head* next_;
    list_head* prev_;
};

inline void INIT_LIST_HEAD(list_head* head) {
    head->next_ = head;
    head->prev_ = head;
}

inline void list_add(list_head* head, list_head* node) {
    node->next_ = head->next_;
    node->prev_ = head;
    head->next_->prev_ = node;
    head->next_ = node;
}

inline void list_del(list_head* node) {
    node->prev_->next_ = node->next_;
    node->next_->prev_ = node->prev_;
}

struct PendingWrite {
    struct list_head node_;

    std::shared_ptr<std::string> data_;
    std::size_t offset_;
    int fd;
};

class PendingWritePool {

public:
    PendingWritePool() 
    { INIT_LIST_HEAD(&free_list_); }

    PendingWrite* get() {
        if(this->free_list_.next_ == &this->free_list_) {
            return new PendingWrite{};
        }

        list_head* node = this->free_list_.next_;
        list_del(node);

        return container_of(node, PendingWrite, node_);
    }

    void release(PendingWrite* pw) {
        reset(pw);
        list_add(&this->free_list_, &pw->node_);
    }

private:
    void reset(PendingWrite* pw) {
        pw->data_.reset();
        pw->offset_ = 0;
        pw->fd = -1;
        
    }

    struct list_head free_list_;

};

class ThreadLocalUring {

public:
    ThreadLocalUring(EventLoop* loop) : loop_(loop) {
        io_uring_params params;
        memset(&params, 0, sizeof(io_uring_params));

        // 开启 SQPOLL，内核线程自动拉取，实现零系统调用
        params.flags |= IORING_SETUP_SQPOLL;
        params.sq_thread_idle = 2000; // 空闲2秒后内核线程休眠

        this->ring_ = new io_uring;
        io_uring_queue_init_params(kEntresLength, this->ring_, &params);

        this->event_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        io_uring_register_eventfd(this->ring_, this->event_fd);

        this->channel_ = std::make_unique<Channel>(this->loop_, this->event_fd);
        this->channel_->setReadCallback([this] (Timestamp timeout) { handleCqes(); });
        this->channel_->enableReading();
    }

    ~ThreadLocalUring() {
        this->channel_->disableAll();
        this->channel_->remove();

        ::close(this->event_fd);
        io_uring_queue_exit(this->ring_);
        delete this->ring_;
    }

    void broadcastInLoop(const std::vector<int>& fds, std::shared_ptr<std::string> message) {
        for(int fd : fds) {
            PendingWrite* pw = this->pool_->get();
            send_schedul(fd, pw);
        }

        io_uring_submit(this->ring_);
    }

private:
    void handleCqes() {
        uint64_t val;
        ::read(this->event_fd, &val, sizeof(val));

        io_uring_cqe* cqe;
        unsigned head;
        unsigned count;

        io_uring_for_each_cqe(ring_, head, cqe) {
            PendingWrite* ctx = static_cast<PendingWrite*>(io_uring_cqe_get_data(cqe));

            if(cqe->res < 0) {
                delete ctx;
                return;

            } else {
                ctx->offset_ += cqe->res;
                send_schedul(ctx->fd, ctx);
            }

            ++count;
        }

        io_uring_cq_advance(this->ring_, count);
    }

    void send_schedul(int fd, PendingWrite* pw) {
        if(this->ring_ == nullptr) return;

        std::size_t remain = pw->data_->size() - pw->offset_;

        if(remain > 0) {
            const char* msg = pw->data_->data() + pw->offset_;

            io_uring_sqe* sqe = get_sqe_safe();
            io_uring_prep_send(sqe, fd, (const void*)msg, remain, MSG_NOSIGNAL);
            io_uring_sqe_set_data(sqe, pw);

        } else {
            this->pool_->release(pw);
        }
    }

    io_uring_sqe* get_sqe_safe() {
        if(this->ring_ == nullptr) return nullptr;

        io_uring_sqe* sqe = io_uring_get_sqe(this->ring_);
        if(!sqe) {
            io_uring_submit(this->ring_);
            sqe = io_uring_get_sqe(this->ring_);
        }

        return sqe;
    }

    EventLoop* loop_;
    io_uring* ring_;
    int event_fd;
    PendingWritePool* pool_;

    std::unique_ptr<Channel> channel_;
};
#pragma once

#include <liburing.h>
#include <cstring>
#include <sys/eventfd.h>
#include <vector>
#include <string>
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include <deque>

#include "muduo/net/EventLoop.h"
#include "muduo/net/Channel.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/base/Logging.h"
#include "websocketConn.h"

using muduo::net::EventLoop;
using muduo::net::Channel;
using muduo::Timestamp;
using muduo::net::TcpConnectionPtr;

#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

constexpr size_t kEntresLength = 16384;

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

    node->next_ = nullptr;
    node->prev_ = nullptr;
}

struct BroadcastTask : public std::enable_shared_from_this<BroadcastTask> {
    std::vector<TcpConnectionPtr> targets_;
    std::shared_ptr<std::string> message_;
    std::size_t current_size_;
    std::size_t chunk_size_;

    BroadcastTask() : current_size_(0), chunk_size_(300) {}
};

struct PendingWrite {
    struct list_head node_;

    std::shared_ptr<std::string> data_;
    std::size_t offset_;
    TcpConnectionPtr conn_;
    bool in_use_;

    PendingWrite() : offset_(0), in_use_(false) {}
};

class PendingWritePool {

public:
    PendingWritePool(std::size_t pool_size) : pool_size_(pool_size)
    { INIT_LIST_HEAD(&free_list_); }

    ~PendingWritePool() {
        while(this->free_list_.next_ != &this->free_list_) {
            PendingWrite* pw = container_of(this->free_list_.next_, PendingWrite, node_);
            list_del(this->free_list_.next_);

            delete pw;
        }
    }

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

    void initPool() {
        for(int i = 0; i < this->pool_size_; ++i) {
            PendingWrite* pw = new PendingWrite{};
            list_add(&this->free_list_, &pw->node_);
        }
    }

private:
    void reset(PendingWrite* pw) {
        if(!pw->in_use_) return;

        pw->in_use_ = false;
        pw->data_.reset();
        pw->conn_.reset();
        pw->offset_ = 0;
    }

    struct list_head free_list_;
    std::size_t pool_size_;
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
        int ret = io_uring_queue_init_params(kEntresLength, this->ring_, &params);

        if (ret < 0) {
            delete this->ring_;
            this->ring_ = nullptr;
            
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf), "io_uring_queue_init_params failed: %s (ret=%d)", strerror(-ret), ret);
            
            LOG_FATAL << err_buf;
        }


        this->event_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        io_uring_register_eventfd(this->ring_, this->event_fd);

        this->channel_ = std::make_unique<Channel>(this->loop_, this->event_fd);
        this->channel_->setReadCallback([this] (Timestamp timeout) { handleCqes(); });
        this->channel_->enableReading();

        this->pool_ = new PendingWritePool(kEntresLength);
        this->pool_->initPool();
    }

    ~ThreadLocalUring() {
        this->channel_->disableAll();
        this->channel_->remove();

        ::close(this->event_fd);
        io_uring_queue_exit(this->ring_);
        delete this->ring_;

        delete this->pool_; 
        this->pool_ = nullptr;
    }

    void broadcastInLoop(const std::vector<WebsocketConnPtr>& conns, std::shared_ptr<std::string> message) {
        if(conns.empty()) return;

        auto task = std::make_shared<BroadcastTask>();
        task->message_ = message;
        task->targets_.reserve(conns.size());

        for(const auto& conn : conns) {
            task->targets_.push_back(conn->conn());
        }

        sendChunk(task);
    }

private:
    void sendChunk(std::shared_ptr<BroadcastTask> task) {
        if(this->ring_ == nullptr || this->pool_ == nullptr) return;

        std::size_t end_idx = std::min(task->current_size_ + task->chunk_size_, task->targets_.size());

        for(std::size_t i = task->current_size_; i < end_idx; ++i) {
            PendingWrite* pw = this->pool_->get();
            pw->data_ = task->message_;
            pw->conn_ = task->targets_[i];
            pw->in_use_ = true;

            send_schedul(pw);
        }

        task->current_size_ = end_idx;

        io_uring_submit(this->ring_);

        if(task->current_size_ < task->targets_.size()) {
            // this->loop_->queueInLoop([task, this] () {
            //     sendChunk(task);
            // });

            this->loop_->runAfter(0.001, [task, this]() {  // 1ms
                sendChunk(task);
            });
        }
    }

    void handleCqes() {
        if (this->ring_ == nullptr || this->pool_ == nullptr) return; 

        uint64_t val;
        ::read(this->event_fd, &val, sizeof(val));

        io_uring_cqe* cqe;
        unsigned head;
        unsigned count{0};

        io_uring_for_each_cqe(ring_, head, cqe) {
            PendingWrite* ctx = static_cast<PendingWrite*>(io_uring_cqe_get_data(cqe));

            if(cqe->res <= 0) {
                this->pool_->release(ctx);
                continue;

            } else {
                ctx->offset_ += cqe->res;
                send_schedul(ctx);
            }

            ++count;
        }

        io_uring_cq_advance(this->ring_, count);

        flush_backlog();
    }

    void send_schedul(PendingWrite* pw) {
        if(this->ring_ == nullptr) return;

        if(!pw->data_ || !pw->conn_) {
            this->pool_->release(pw);
            return;
        }

        if(!pw->conn_->connected()) {
            this->pool_->release(pw);
            return;
        }

        std::size_t remain = pw->data_->size() - pw->offset_;

        if(remain > 0) {
            io_uring_sqe* sqe = io_uring_get_sqe(this->ring_);
            if(!sqe) {
                this->backlog_.push_back(pw);
                io_uring_submit(this->ring_);

                return;
            }

            const char* msg = pw->data_->data() + pw->offset_;
            io_uring_prep_send(sqe, pw->conn_->fd(), (const void*)msg, remain, MSG_NOSIGNAL);
            io_uring_sqe_set_data(sqe, pw);

        } else {
            this->pool_->release(pw);
        }
    }

    void flush_backlog() {
        if(this->backlog_.empty()) return;

        int processed = 0;
        while(!this->backlog_.empty()) {
            io_uring_sqe* sqe = io_uring_get_sqe(this->ring_);
            if(!sqe) break;

            PendingWrite* pw = this->backlog_.front();
            this->backlog_.pop_front();

            if(!pw->data_ || !pw->conn_ || !pw->conn_->connected()) {
                this->pool_->release(pw);
                continue;
            }

            std::size_t remain = pw->data_->size() - pw->offset_;
            const char* msg = pw->data_->data() + pw->offset_;

            io_uring_prep_send(sqe, pw->conn_->fd(), (const void*)msg, remain, MSG_NOSIGNAL);
            io_uring_sqe_set_data(sqe, pw);

            ++processed;
        }

        if(processed > 0) {
            io_uring_submit(this->ring_);
        }
    }

    EventLoop* loop_;
    io_uring* ring_;
    int event_fd;
    PendingWritePool* pool_;
    std::deque<PendingWrite*> backlog_;

    std::unique_ptr<Channel> channel_;
};
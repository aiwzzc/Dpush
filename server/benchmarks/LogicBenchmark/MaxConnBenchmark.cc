#include "../../GatewayServer/muduo/net/TcpServer.h"
#include "../../GatewayServer/muduo/net/EventLoop.h"
#include "../../GatewayServer/muduo/net/TcpClient.h"
#include "../../GatewayServer/muduo/net/EventLoopThreadPool.h"
#include "../../GatewayServer/muduo/net/TcpConnection.h"
#include "../../GatewayServer/muduo/net/Buffer.h"
#include "../../GatewayServer/muduo/base/Timestamp.h"
#include "../../GatewayServer/muduo/base/Logging.h"
#include "../../flatbuffers/chat_generated.h"

using muduo::net::TcpServer;
using muduo::net::EventLoop;
using muduo::net::InetAddress;
using muduo::net::TcpClient;
using muduo::net::EventLoopThreadPool;
using muduo::net::TcpConnectionPtr;
using muduo::net::Buffer;
using muduo::Timestamp;
using muduo::Logger;

#include <vector>
#include <atomic>
#include <memory>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <array>
#include <thread>
#include <climits>

/*

GET /ws HTTP/1.1
Host: 192.168.183.130:5005
Connection: Upgrade
Pragma: no-cache
Cache-Control: no-cache
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36
Upgrade: websocket
Origin: http://192.168.183.130:5005
Sec-WebSocket-Version: 13
Accept-Encoding: gzip, deflate
Accept-Language: zh-CN,zh;q=0.9,en;q=0.8
Cookie: sid=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3NzQ0MTc2MDMsInVzZXJpZCI6MjYsInVzZXJuYW1lIjoia2luZyJ9.MVNZp2rGg55XvqFhmcdrBttY2Gas37TE5ACKONcQiqCpDQBNBD5dTea3WcO1aAZe4vXJvGtT62CKWeZ--vGSc8hwFOcBdl5BC9LM5yykFaoLupCipdG7ydfZnpfFeJrLWprhQ-A27NaNXgWndSOgrHSR3y-QvMSyRBAkOAV4bCZ0dGZVhcQiMtB-gse0Z-k5eIqb2ryavpi8DmD2M8P4V5opr65QADdaLYNZi2rvxwRYAiJo2eYfNlf2MLZl-Xzn3JlNIqnqLLQ0dXCKUfivPGr-or3P5yuweqoMBqFLloEJ3rnkpTLUgJ_XZL38IOMeYfNZMNYNwM_X606YA8nrEQ
Sec-WebSocket-Key: NKjXvd3uVRzql3IvEKX/YQ==
Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits
*/

constexpr std::size_t LoopThreadNum = 6;
constexpr std::size_t ConnNum       = 100000;

static inline long long getCurrentTimestamp() {
    auto now = std::chrono::system_clock::time_point::clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return milliseconds.count(); //单位是毫秒
}

std::pair<const uint8_t*, size_t> parseWebsocket(Buffer* buf) {
    if (buf->readableBytes() < 2) return {nullptr, 0};

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(buf->peek());
    uint64_t payload_length = bytes[1] & 0x7F;
    size_t offset = 2;

    if (payload_length == 126) {
        if (buf->readableBytes() < 4) return {nullptr, 0};
        payload_length = (bytes[2] << 8) | bytes[3];
        offset += 2;
    } else if (payload_length == 127) {
        if (buf->readableBytes() < 10) return {nullptr, 0};
        payload_length = 0;
        for(int i = 0; i < 8; ++i) {
            payload_length = (payload_length << 8) | bytes[2+i];
        }
        offset += 8;
    }

    // 检查整个帧是否完整
    if (buf->readableBytes() < offset + payload_length) {
        return {nullptr, 0}; 
    }

    const uint8_t* payload_ptr = bytes + offset;
    
    // 注意：这里不要再 buf->retrieve 了！
    // 交给外层函数在使用完指针后统一 retrieve，防止悬空指针
    return {payload_ptr, payload_length};
}

class Performance {

public:
    Performance() {
        this->first_recv_ts.store(LONG_MAX);
        this->last_recv_ts.store(0);
    }

    ~Performance() {
        std::lock_guard<std::mutex> lock(this->registry_mtx_);
        for(auto* local_vec : this->tls_registry_) {
            delete local_vec;
        }

        this->tls_registry_.clear();
    }

    void onConnection(const TcpConnectionPtr& conn) {
        if(conn->connected()) {
            this->successNum_.fetch_add(1, std::memory_order_relaxed);

            std::string userid_str = std::to_string(this->userid.load());
            this->userid++;

            std::string websocket =
                "GET " +  userid_str + " HTTP/1.1\r\n"
                "Host: 192.168.183.130:5005\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: NKjXvd3uVRzql3IvEKX/YQ==\r\n\r\n";

            conn->send(websocket);

        } else {
            this->failedNum_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
        long long current_time = getCurrentTimestamp();

        thread_local std::vector<long long>* local_latencies = nullptr;
        if(local_latencies == nullptr) {
            local_latencies = new std::vector<long long>();

            local_latencies->reserve((100000 / LoopThreadNum) + 5000);

            std::lock_guard<std::mutex> lock(this->registry_mtx_);
            this->tls_registry_.push_back(local_latencies);
        }

        while(buf->readableBytes() > 0) {
            auto[payload_ptr, payload_length] = parseWebsocket(buf);
            if(payload_ptr == nullptr) return;

            size_t header_size = (payload_length > 65535) ? 10 : (payload_length > 125 ? 4 : 2);
            size_t total_size = header_size + payload_length;

            auto rootMsg = ChatApp::GetRootMessage(payload_ptr);
            if (rootMsg) {
                auto payload = rootMsg->payload_as_ServerMessagePayload();
                if (payload && payload->messages() && payload->messages()->size() > 0) {
                    
                    long long server_send_ts{payload->messages()->begin()->timestamp()};
                    long long latency = current_time - server_send_ts;

                    local_latencies->push_back(latency);

                    updateMaxMin(current_time);
                    recvNum_.fetch_add(1, std::memory_order_relaxed);
                }
            }

            buf->retrieve(total_size);
        }
    }

    void printStats() {
        LOG_WARN << "Connected: " << successNum_.load() 
                 << " | Disconnected: " << failedNum_.load()
                 << " | Total Msgs Recv: " << recvNum_.load();

        if(this->recvNum_ >= 100000 && !this->calc_done_) {
            calculateMetrics();
            this->calc_done_ = true;
        }
    }

private:
    void updateMaxMin(long long current_time) {
        long long max_val = this->last_recv_ts.load(std::memory_order_relaxed);
        while(current_time > max_val && 
            !last_recv_ts.compare_exchange_weak(max_val, current_time, std::memory_order_relaxed)) {}

        long long min_val = first_recv_ts.load(std::memory_order_relaxed);
        while(current_time < min_val && 
            !first_recv_ts.compare_exchange_weak(min_val, current_time, std::memory_order_relaxed)) {}
    }

    void calculateMetrics() {
        std::vector<long long> all_latencys;
        all_latencys.reserve(100000);

        {
            std::lock_guard<std::mutex> lock(this->registry_mtx_);
            for(const auto& latencys : this->tls_registry_) {
                all_latencys.insert(all_latencys.end(), latencys->begin(), latencys->end());
            }
        }

        if(all_latencys.empty()) return;

        long long broadcast_duration = this->last_recv_ts.load() - this->first_recv_ts.load();

        std::sort(all_latencys.begin(), all_latencys.end());

        long long p50 = all_latencys[all_latencys.size() * 0.50];
        long long p95 = all_latencys[all_latencys.size() * 0.95];
        long long p99 = all_latencys[all_latencys.size() * 0.99];
        long long p999 = all_latencys[all_latencys.size() * 0.999];
        long long max_lat = all_latencys.back();

        LOG_WARN << "================ PERFORMANCE REPORT ================";
        LOG_WARN << "Total Broadcast Duration : " << broadcast_duration << " ms";
        LOG_WARN << "Average Throughput       : " << (100000.0 / broadcast_duration * 1000) << " msgs/sec";
        LOG_WARN << "Latency P50              : " << p50 << " ms";
        LOG_WARN << "Latency P95              : " << p95 << " ms";
        LOG_WARN << "Latency P99              : " << p99 << " ms";
        LOG_WARN << "Latency P99.9            : " << p999 << " ms";
        LOG_WARN << "Latency Max              : " << max_lat << " ms";
        LOG_WARN << "====================================================";
    }

    std::atomic<int> userid{33};

    std::atomic<long long> successNum_{0};
    std::atomic<long long> failedNum_{0};
    std::atomic<long long> recvNum_{0};

    std::atomic<long long> first_recv_ts;
    std::atomic<long long> last_recv_ts;
    bool calc_done_{false};

    static constexpr const int SHARD_COUNT = 32;

    // 保护注册表的锁（仅在线程初始化时触发几次，对性能毫无影响）
    std::mutex registry_mtx_;
    // 存储所有 IO 线程各自的 latencies 数组的指针
    std::vector<std::vector<long long>*> tls_registry_;
};

int main(int agrc, char** agrv) {
    Logger::setLogLevel(muduo::Logger::WARN);

    EventLoop loop{};

    EventLoopThreadPool loops{&loop, "clientLoops"};
    loops.setThreadNum(LoopThreadNum);
    loops.start();

    Performance perfMonitor;

    std::vector<std::unique_ptr<TcpClient>> clientServers;
    clientServers.reserve(ConnNum);

    LOG_WARN << "Initializing " << ConnNum << " clients...";

    for(int i = 0; i < ConnNum; ++i) {
        EventLoop* loop = loops.getNextLoop();

        char name[32];
        snprintf(name, sizeof name, "client_%zu", i);

        uint16_t port = 5003 + (i % 3);
        InetAddress addr("192.168.183.130", port);

        auto clientServer = std::make_unique<TcpClient>(loop, addr, name);

        clientServer->setConnectionCallback([&perfMonitor] (const TcpConnectionPtr& conn) {
            conn->setTcpNoDelay(true);
            perfMonitor.onConnection(conn);
        });

        clientServer->setMessageCallback([&perfMonitor] (const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
            perfMonitor.onMessage(conn, buf, time);
        });

        clientServers.emplace_back(std::move(clientServer));
    }

    loop.runEvery(1.0, [&perfMonitor] () {
        perfMonitor.printStats();
    });

    std::thread connectThread([&clientServers]() {
        for(int i = 0; i < ConnNum; ++i) {
            clientServers[i]->connect();
            if (i % 5000 == 0) LOG_WARN << "Initiated " << i << " connections...";
            usleep(1000);
        }
    });

    loop.loop();
    connectThread.join();

    return 0;
}
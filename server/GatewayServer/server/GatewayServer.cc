#include "GatewayServer.h"
#include "producer.h"
#include "iouring.h"
#include "handleHttpEvent.h"

static char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char* buf = (char*)malloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, len, f);
    buf[len] = '\0';  // 一定要加

    fclose(f);
    return buf;
}

extern thread_local std::unique_ptr<ThreadLocalUring> t_uring_ptr;

// GatewayServer::GatewayServer() : HttpServer_(std::make_unique<HttpServer>(muduo::net::InetAddress{"0.0.0.0", 5005}, "HttpServer", 6)), 
// grpcClient_(std::make_shared<grpcClient>()),
// GatewayPubSubManager_(std::make_unique<GatewayPubSubManager>()), kafkaProducer_(std::make_shared<kafkaProducer>()) {
//     this->HttpServer_->setHttpCallback([this] (TcpConnectionPtr conn, HttpRequest req) {
//         handleHttpEvent(conn, req, this->grpcClient_);
//     });

//     this->HttpServer_->setUpgradeCallback([this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
//         handleUpgradeEvent(conn, req, this->grpcClient_, this->kafkaProducer_);
//     });

    // this->HttpServer_->setThreadInitCallback([] (EventLoop* loop) {
    //     t_uring_ptr = std::make_unique<ThreadLocalUring>(loop);

    //     GatewayPubSubManager::RegisterLoop(loop);
    // });

// }

GatewayServer::GatewayServer() : HttpServer_(std::make_unique<HttpServer>(5003, 3, "HttpServer", 9)), 
grpcClient_(std::make_shared<grpcClient>()),
GatewayPubSubManager_(std::make_unique<GatewayPubSubManager>()), kafkaProducer_(std::make_shared<kafkaProducer>()) {
    this->HttpServer_->setHttpCallback([this] (TcpConnectionPtr conn, HttpRequest req) {
        HttpEventHandlers::getInstance().handleHttpEvent(conn, req, this->grpcClient_);
    });

    this->HttpServer_->setUpgradeCallback([this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
        handleUpgradeEvent(conn, req, this->grpcClient_, this->kafkaProducer_);
    });

    this->HttpServer_->setThreadInitCallback([] (EventLoop* loop) {
        t_uring_ptr = std::make_unique<ThreadLocalUring>(loop);

        GatewayPubSubManager::RegisterLoop(loop);
    });
}

const char* GatewayServer::public_key = read_file("/home/zzc/linux_test/DistributedPush/server/GatewayServer/base/public.pem");

GatewayServer::~GatewayServer() {
    if(this->poolthread_.joinable()) this->poolthread_.join();
}

void GatewayServer::start() {
    this->poolthread_ = std::thread([this] {
        while(1) {
            this->kafkaProducer_->poll(5);
        }
    });

    this->HttpServer_->start();
}
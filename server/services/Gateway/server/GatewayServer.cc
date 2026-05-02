#include "GatewayServer.h"
#include "producer.h"
#include "iouring.h"
#include "handleHttpEvent.h"
#include "heartbeatManager.h"
#include "config.h"

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
extern thread_local std::unique_ptr<heartbeatManager> t_heartbeatManager_ptr;

std::unordered_map<int32_t, muduo::net::EventLoop*> GatewayServer::user_Eventloop_{};
std::shared_mutex GatewayServer::user_Eventloop_mutex_{};

GatewayServer::GatewayServer() : 
HttpServer_(std::make_unique<HttpServer>(muduo::net::InetAddress{"0.0.0.0", (uint16_t)Config::getInstance().port_}, "HttpServer", 6)), 
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
        t_heartbeatManager_ptr = std::make_unique<heartbeatManager>();
        loop->runEvery(30, [] () {
            t_heartbeatManager_ptr->onTimerTick();
        });

        GatewayPubSubManager::RegisterLoop(loop);
    });

    this->grpcServer_ = std::make_unique<GatewayGrpcServer>();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(Config::getInstance().listen_addr_, grpc::InsecureServerCredentials());
    builder.RegisterService(this->grpcServer_.get());

    this->register_ = std::make_unique<GatewayRegister>("127.0.0.1:2379", Config::getInstance().addr_);
}

// test mode
#if 0
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
        t_heartbeatManager_ptr = std::make_unique<heartbeatManager>();
        loop->runEvery(30, [] () {
            t_heartbeatManager_ptr->onTimerTick();
        });

        GatewayPubSubManager::RegisterLoop(loop);
    });

    this->grpcServer_ = std::make_unique<GatewayGrpcServer>();

    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:5005", grpc::InsecureServerCredentials());
    builder.RegisterService(this->grpcServer_.get());

    this->register_ = std::make_unique<GatewayRegister>("127.0.0.1:2379", Config::getInstance().addr_);
}
#endif

const char* GatewayServer::public_key = read_file("/home/zzc/linux_test/DistributedPush/server/config/public.pem");

GatewayServer::~GatewayServer() {
    if(this->poolthread_.joinable()) this->poolthread_.join();
    this->register_->stop();
}

void GatewayServer::start() {
    this->poolthread_ = std::thread([this] {
        while(1) {
            this->kafkaProducer_->poll(5);
        }
    });

    this->register_->start();
    this->HttpServer_->start();
}
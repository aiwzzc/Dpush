#include "GatewayServer.h"
#include "producer.h"
#include "iouring.h"
#include "heartbeatManager.h"
#include "config.h"
#include "yyjson/JsonView.h"

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
std::atomic<long> GatewayServer::conned_count_{0};

GatewayServer::GatewayServer() : 
grpcClient_(std::make_unique<grpcClient>()), GatewayPubSubManager_(std::make_unique<GatewayPubSubManager>()), 
kafkaProducer_(std::make_unique<kafkaProducer>()) {

    WsServerContext ctx{this->grpcClient_.get(), this->kafkaProducer_.get()};

    this->wsServer_ = std::make_unique<wsServer>(
    muduo::net::InetAddress{"0.0.0.0", (uint16_t)Config::getInstance().port_}, 
    "wsServer", 6, ctx);

    this->wsServer_->setThreadInitCallback([] (EventLoop* loop) {
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

    this->load_queue_ = std::make_unique<moodycamel::ConcurrentQueue<GatewayLoad>>();

    this->register_ = std::make_unique<GatewayRegister>("127.0.0.1:2379", Config::getInstance().addr_);

    sw::redis::ConnectionOptions connection_options;
    connection_options.host = "127.0.0.1";
    connection_options.port = 6379;
    connection_options.db = 1;

    sw::redis::ConnectionPoolOptions pool_options;
    pool_options.size = 6;

    this->redisPool_ = std::make_unique<sw::redis::Redis>(connection_options, pool_options);
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
    this->running_ = false;
    if(this->poolthread_.joinable()) this->poolthread_.join();
    if(this->redis_worker_.joinable()) this->redis_worker_.join();
    this->register_->stop();
}

static uint64_t getCurrentTimestamp() {
    auto now = std::chrono::system_clock::time_point::clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return milliseconds.count(); //单位是毫秒
}

static std::string build_load_json(GatewayLoad& load) {
    JsonDoc root;
    root.root()["conn"].set(load.conn_count);
    root.root()["ts"].set(std::to_string(load.update_time));
    return root.toString();
}

void GatewayServer::collect_load_to_spsc() {
    GatewayLoad load;

    load.conn_count = this->conned_count_.load(std::memory_order_relaxed);
    load.update_time = getCurrentTimestamp();

    this->load_queue_->enqueue(load);
}

void GatewayServer::write_load_to_redis(GatewayLoad& load) {
    this->redisPool_->hset("gateway:load", Config::getInstance().addr_, build_load_json(load));
}

void GatewayServer::start() {
    this->poolthread_ = std::thread([this] {
        while(this->running_) {
            this->kafkaProducer_->poll(5);
        }
    });

    this->redis_worker_ = std::thread([this] () {
        GatewayLoad load;

        while(this->running_) {
            if(this->load_queue_->try_dequeue(load)) {
                this->write_load_to_redis(load);
            }
        }
    });

    this->wsServer_->setMainLoopTimerCallback([this] () {
        this->collect_load_to_spsc();
    });

    this->register_->start();
    this->wsServer_->start();
}
#include "LogicServer.h"
#include "constants/netAddress.h"
#include "constants/err_code.h"

LogicServer::LogicServer(pulse::config::LogicConfig& config):
config_(std::move(config)),
serviceRegistryClient_(this->config_.infra.etcdConfig.endpoints.front()) {

    this->serviceRegistryClient_.RegisterSelf(
        this->config_.infra.serviceRegistryConfig.logic_service_prefix,
        this->config_.endpoint
    );

    auto mysql_endpoints = this->config_.infra.mysqlConfig.endpoints;
    std::string mysql_host = mysql_endpoints.front().host;
    std::string mysql_port_str = std::to_string(mysql_endpoints.front().port);

    mysql_info info{
        mysql_host, 
        mysql_port_str, 
        this->config_.infra.mysqlConfig.database, 
        this->config_.infra.mysqlConfig.username, 
        this->config_.infra.mysqlConfig.password
    };

    this->mysql_cluster_ = std::make_unique<asyncMysqlCluster>(
        this->config_.infra.mysqlConfig.pool_thread_count, 
        this->config_.infra.mysqlConfig.pool_size, 
        info
    );

    auto redis_endpoints = this->config_.infra.redisConfig.endpoints;

    sw::redis::ConnectionOptions connection_options;
    connection_options.host = redis_endpoints.front().host;
    connection_options.port = redis_endpoints.front().port;
    connection_options.db = this->config_.infra.redisConfig.db;

    sw::redis::ConnectionPoolOptions pool_options;
    pool_options.size = this->config_.infra.redisConfig.pool_size;

    this->redisPool_ = std::make_unique<sw::redis::Redis>(connection_options, pool_options);

    this->ComputeThreadPool_ = 
    std::make_unique<ComputeThreadPool>(this->config_.concurrency.thread_pool_size);
    this->OrderedThreadPool_ = 
    std::make_unique<OrderedThreadPool>(this->config_.concurrency.thread_pool_size);

    this->LogicGrpcService_ = std::make_unique<LogicGrpcServer>(this->mysql_cluster_.get(), 
    this->redisPool_.get(), this->ComputeThreadPool_.get());

    pulse::net::NetAddress grpc_listen_addr = 
    pulse::net::NetAddress::AnyAddr(this->config_.service.port);

    ServerBuilder builder;
    builder.AddListeningPort(grpc_listen_addr.GetUrl(), grpc::InsecureServerCredentials());
    builder.RegisterService(this->LogicGrpcService_.get());

    this->LogicGrpcServer_ = std::move(builder.BuildAndStart());

    if(!this->LogicGrpcServer_) {
        throw std::runtime_error(pulse::constants::err::KGrpcFailedStartErrorMsg.data());
    }

    this->KafkaConsumer_ = std::make_unique<KafkaConsumer>(
        this->config_.infra.kafkaConfig.endpoints.front(), 
        this->config_.infra.kafkaConfig.consumer.group_id, 
        this->config_.kafkaConsumerConfig.topics, 
        this->OrderedThreadPool_.get(), 
        this->ComputeThreadPool_.get(), 
        this->redisPool_.get()
    );

    this->grpc_client_ = std::make_unique<grpcClient>(this->config_, &this->serviceRegistryClient_);
}

LogicServer::~LogicServer() { this->KafkaConsumer_->stop(); }

void LogicServer::start() {
    this->ComputeThreadPool_->start();
    this->OrderedThreadPool_->start();

    this->mysql_cluster_->start();

    this->KafkaConsumer_->setgrpcClient(this->grpc_client_.get());
    this->KafkaConsumer_->start();

    this->LogicGrpcServer_->Wait();
}
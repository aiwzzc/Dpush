#include "LogicServer.h"

LogicServer::LogicServer() {
    this->MySQLConnPool_.reset(MySQLConnPool::getinstance(LogicServer::dbname));
    this->MySQLConnPool_->initpool(LogicServer::url, LogicServer::pool_size);

    sw::redis::ConnectionOptions connection_options;
    connection_options.host = "127.0.0.1";
    connection_options.port = 6379;
    connection_options.db = 1;

    sw::redis::ConnectionPoolOptions pool_options;
    pool_options.size = 2;

    this->redisPool_ = std::make_unique<sw::redis::Redis>(connection_options, pool_options);

    this->ComputeThreadPool_ = std::make_unique<ComputeThreadPool>(4);
    this->OrderedThreadPool_ = std::make_unique<OrderedThreadPool>(4);

    this->LogicGrpcService_ = std::make_unique<LogicGrpcServer>(this->MySQLConnPool_.get(), 
    this->redisPool_.get(), this->ComputeThreadPool_.get());

    ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:5008", grpc::InsecureServerCredentials());
    builder.RegisterService(this->LogicGrpcService_.get());

    this->LogicGrpcServer_ = std::move(builder.BuildAndStart());

    if(!this->LogicGrpcServer_) {
        throw std::runtime_error("Failed to start gRPC server! Please check port or configuration.");
    }

    // this->KafkaConsumer_ = std::make_unique<KafkaConsumer>(LogicServer::brokers, LogicServer::groupId, std::vector<std::string>{});
}

LogicServer::~LogicServer() = default;

void LogicServer::start() {
    this->ComputeThreadPool_->start();
    this->OrderedThreadPool_->start();

    this->LogicGrpcServer_->Wait();
}
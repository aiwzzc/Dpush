#include "LogicServer.h"

LogicServer::LogicServer() {
    this->mysql_cluster_ = std::make_unique<asyncMysqlCluster>(4, 10);

    sw::redis::ConnectionOptions connection_options;
    connection_options.host = "127.0.0.1";
    connection_options.port = 6379;
    connection_options.db = 1;

    sw::redis::ConnectionPoolOptions pool_options;
    pool_options.size = 2;

    this->redisPool_ = std::make_unique<sw::redis::Redis>(connection_options, pool_options);

    this->ComputeThreadPool_ = std::make_unique<ComputeThreadPool>(6);
    this->OrderedThreadPool_ = std::make_unique<OrderedThreadPool>(6);

    this->LogicGrpcService_ = std::make_unique<LogicGrpcServer>(this->mysql_cluster_.get(), 
    this->redisPool_.get(), this->ComputeThreadPool_.get());

    ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:5008", grpc::InsecureServerCredentials());
    builder.RegisterService(this->LogicGrpcService_.get());

    this->LogicGrpcServer_ = std::move(builder.BuildAndStart());

    if(!this->LogicGrpcServer_) {
        throw std::runtime_error("Failed to start gRPC server! Please check port or configuration.");
    }

    std::vector<std::string> topics{"chat_room_messages"};

    this->KafkaConsumer_ = std::make_unique<KafkaConsumer>(LogicServer::brokers, LogicServer::groupId, topics, 
    this->OrderedThreadPool_.get(), this->ComputeThreadPool_.get(), this->redisPool_.get());

    this->discover_ = std::make_unique<LogicDiscovery>("127.0.0.1:2379");

    this->grpc_client_ = std::make_unique<grpcClient>(this->discover_.get());
}

LogicServer::~LogicServer() { this->KafkaConsumer_->stop(); }

void LogicServer::start() {
    this->ComputeThreadPool_->start();
    this->OrderedThreadPool_->start();

    this->mysql_cluster_->start();

    this->KafkaConsumer_->setgrpcClient(this->grpc_client_.get());
    this->KafkaConsumer_->start();

    this->discover_->start();
    this->LogicGrpcServer_->Wait();
}
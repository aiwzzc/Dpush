#include "GatewayServer.h"
#include "producer.h"

GatewayServer::GatewayServer() : HttpServer_(std::make_unique<HttpServer>(2)), grpcClient_(std::make_shared<grpcClient>()),
GatewayPubSubManager_(std::make_unique<GatewayPubSubManager>()), kafkaProducer_(std::make_shared<kafkaProducer>()) {
    this->HttpServer_->setHttpCallback([this] (TcpConnectionPtr conn, HttpRequest req) {
        handleHttpEvent(conn, req, this->grpcClient_);
    });

    this->HttpServer_->setUpgradeCallback([this] (const TcpConnectionPtr& conn, const HttpRequest& req) {
        handleUpgradeEvent(conn, req, this->grpcClient_, this->kafkaProducer_);
    });
}
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
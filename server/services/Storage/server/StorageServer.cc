#include "StorageServer.h"

StorageServer::StorageServer(int thread_count) {

    mysql_info info{"127.0.0.1", "3306", "chatroom", "root", "zzc1109aiw"};

    this->mysql_cluster_ = std::make_unique<asyncMysqlCluster>(thread_count, 1, info);

    std::vector<std::string> topics{"chat_room_messages"};

    this->consumer_ = std::make_unique<storageConsumer>(this->mysql_cluster_.get(),
    StorageServer::brokers, StorageServer::groupId, topics);
}

void StorageServer::start() {
    this->mysql_cluster_->start();
    this->consumer_->start();
}
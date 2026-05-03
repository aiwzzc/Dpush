#pragma once

#include <memory>

#include "storageConsumer.h"
#include "storage/AsyncMySQLConnPool/AsyncMysqlCluster.h"

class StorageServer {

public:
    StorageServer(int thread_count);

    void start();

private:
    static constexpr const char* brokers           = "localhost:9092";
    static constexpr const char* groupId           = "chat_room_consumer_group_1";

    std::unique_ptr<asyncMysqlCluster> mysql_cluster_;
    std::unique_ptr<storageConsumer> consumer_;

};
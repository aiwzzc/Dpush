#pragma once

#include "../../proto/logic.grpc.pb.h"
#include "../../proto/logic.pb.h"

#include "../../AuthServer/mysql/BlockQueue.h"
#include "../../AuthServer/mysql/MySQLConn.h"
#include "../../AuthServer/mysql/MySQLConnPool.h"
#include "../../AuthServer/mysql/MySQLWorker.h"
#include "../../AuthServer/mysql/SQLOperation.h"

#include "../base/coroutineTask.h"
#include "../base/threadPool.h"

#include <vector>
#include <grpcpp/grpcpp.h>
#include <sw/redis++/redis++.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerReaderWriter;
using grpc::Status;

class LogicServer final : public logic::LogicServer::CallbackService {

public:
    using streamMsg = std::pair<std::string, std::unordered_map<std::string, std::string>>;

    LogicServer(MySQLConnPool*, sw::redis::Redis*, threadpool*);
    ~LogicServer();

    grpc::ServerUnaryReactor* initialPullMessage(grpc::CallbackServerContext* context, const logic::pullMessageRequest* request, 
        logic::pullMessageResponse* response) override;

    grpc::ServerUnaryReactor* clientMessage(grpc::CallbackServerContext* context, const logic::clientMessageRequest* request, 
        logic::clientMessageResponse* response) override;

    grpc::ServerUnaryReactor* clearCursors(grpc::CallbackServerContext* context, const logic::clearCursorsRequest* request, 
        logic::clearCursorsResponse* response) override;

private:
    DetachedTask DoinitialPullMessage(grpc::ServerUnaryReactor*, const logic::pullMessageRequest*, 
        logic::pullMessageResponse*);

    DetachedTask DoclientMessage(grpc::ServerUnaryReactor*, const logic::clientMessageRequest*, 
        logic::clientMessageResponse*);

    DetachedTask DoclearCursors(grpc::ServerUnaryReactor*, const logic::clearCursorsRequest*, 
        logic::clearCursorsResponse*);

    MySQLConnPool* mysql_pool_;
    sw::redis::Redis* redis_pool_;
    threadpool* thread_pool_;
};
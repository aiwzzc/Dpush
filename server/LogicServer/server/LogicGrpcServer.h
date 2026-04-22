#pragma once

#include "../../proto/logic.grpc.pb.h"
#include "../../proto/logic.pb.h"

#include "../../AuthServer/mysql/BlockQueue.h"
#include "../../AuthServer/mysql/MySQLConn.h"
#include "../../AuthServer/mysql/MySQLConnPool.h"
#include "../../AuthServer/mysql/MySQLWorker.h"
#include "../../AuthServer/mysql/SQLOperation.h"

#include "../base/coroutineTask.h"
#include "../base/ComputeThreadPool.h"

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

class LogicGrpcServer final : public logic::LogicServer::CallbackService {

public:
    using streamMsg = std::pair<std::string, std::unordered_map<std::string, std::string>>;

    LogicGrpcServer(MySQLConnPool*, sw::redis::Redis*, ComputeThreadPool*);
    ~LogicGrpcServer();

    grpc::ServerUnaryReactor* initialPullMessage(grpc::CallbackServerContext* context, const logic::pullMessageRequest* request, 
        logic::pullMessageResponse* response) override;

    grpc::ServerUnaryReactor* clientMessage(grpc::CallbackServerContext* context, const logic::clientMessageRequest* request, 
        logic::clientMessageResponse* response) override;

    grpc::ServerUnaryReactor* clearCursors(grpc::CallbackServerContext* context, const logic::clearCursorsRequest* request, 
        logic::clearCursorsResponse* response) override;

    grpc::ServerWriteReactor<logic::bathPullMessageResponse>* bathPullMessage(grpc::CallbackServerContext* context,
    const logic::bathPullMessageRequest* request) override;

    grpc::ServerUnaryReactor* joinSession(grpc::CallbackServerContext* context, const logic::joinSessionRequest* request, 
        logic::joinSessionResponse* response) override;

private:
    DetachedTask DoinitialPullMessage(grpc::ServerUnaryReactor*, const logic::pullMessageRequest*, 
        logic::pullMessageResponse*);

    DetachedTask DoclientMessage(grpc::ServerUnaryReactor*, const logic::clientMessageRequest*, 
        logic::clientMessageResponse*);

    DetachedTask DoclearCursors(grpc::ServerUnaryReactor*, const logic::clearCursorsRequest*, 
        logic::clearCursorsResponse*);

    DetachedTask DojoinSession(grpc::ServerUnaryReactor*, const logic::joinSessionRequest*, 
        logic::joinSessionResponse*);

    MySQLConnPool* mysql_pool_;
    sw::redis::Redis* redis_pool_;
    ComputeThreadPool* thread_pool_;
};

class BatchPullReactor : public ::grpc::ServerWriteReactor<logic::bathPullMessageResponse> {

public:
    BatchPullReactor(const logic::bathPullMessageRequest*, sw::redis::Redis*, ComputeThreadPool*);

    void OnWriteDone(bool ok) override;
    void OnDone() override;
private:
    DetachedTask FetchNextAndWrite();

    const logic::bathPullMessageRequest* request_;
    logic::bathPullMessageResponse response_;
    sw::redis::Redis* redis_pool_;
    ComputeThreadPool* thread_pool_;
    std::size_t current_index_{0};
};
#pragma once

#include "logic.grpc.pb.h"
#include "logic.pb.h"

#include "storage/mysql/BlockQueue.h"
#include "storage/mysql/MySQLConn.h"
#include "storage/mysql/MySQLConnPool.h"
#include "storage/mysql/MySQLWorker.h"
#include "storage/mysql/SQLOperation.h"

#include "coroutineTask.h"
#include "ComputeThreadPool.h"

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

    grpc::ServerUnaryReactor* clientMessage(grpc::CallbackServerContext* context, const logic::clientMessageRequest* request, 
        logic::clientMessageResponse* response) override;

    grpc::ServerUnaryReactor* clearCursors(grpc::CallbackServerContext* context, const logic::clearCursorsRequest* request, 
        logic::clearCursorsResponse* response) override;

    grpc::ServerWriteReactor<logic::bathPullMessageResponse>* bathPullMessage(grpc::CallbackServerContext* context,
    const logic::bathPullMessageRequest* request) override;

    grpc::ServerUnaryReactor* joinSession(grpc::CallbackServerContext* context, const logic::joinSessionRequest* request, 
        logic::joinSessionResponse* response) override;

    grpc::ServerUnaryReactor* createSession(grpc::CallbackServerContext* context, const logic::createSessionRequest* request,
        logic::createSessionResponse* response) override;

    grpc::ServerUnaryReactor* pullMessage(grpc::CallbackServerContext* context, const logic::PullMessageRequest* request, 
        logic::PullMessageResponse* response) override;

private:
    DetachedTask DoclientMessage(grpc::ServerUnaryReactor*, const logic::clientMessageRequest*, 
        logic::clientMessageResponse*);

    DetachedTask DoclearCursors(grpc::ServerUnaryReactor*, const logic::clearCursorsRequest*, 
        logic::clearCursorsResponse*);

    DetachedTask DojoinSession(grpc::ServerUnaryReactor*, const logic::joinSessionRequest*, 
        logic::joinSessionResponse*);

    DetachedTask DocreateSession(grpc::ServerUnaryReactor*, const logic::createSessionRequest*, 
        logic::createSessionResponse*);

    DetachedTask DoPullMessage(grpc::ServerUnaryReactor*, const logic::PullMessageRequest*, 
        logic::PullMessageResponse*);

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
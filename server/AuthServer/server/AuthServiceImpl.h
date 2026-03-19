#pragma once

#include "../../proto/auth.grpc.pb.h"
#include "../../proto/auth.pb.h"

#include "../base/coroutineTask.h"
#include "../base/md5.h"

#include "mysql/BlockQueue.h"
#include "mysql/MySQLConn.h"
#include "mysql/MySQLConnPool.h"
#include "mysql/MySQLWorker.h"
#include "mysql/SQLOperation.h"

#include <memory>
#include <grpcpp/grpcpp.h>
#include <sw/redis++/redis++.h>
#include <uuid/uuid.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerReaderWriter;
using grpc::Status;

class AuthService final : public auth::AuthServer::CallbackService {

public:
    AuthService(MySQLConnPool* mysqlpool, sw::redis::Redis* redispool);
    ~AuthService();

    grpc::ServerUnaryReactor* Login(grpc::CallbackServerContext* context, const auth::LoginRequest* request, 
        auth::LoginResponse* response) override;

    grpc::ServerUnaryReactor* Register(grpc::CallbackServerContext* context, const auth::RegisterRequest* request, 
        auth::RegisterResponse* response) override;

    grpc::ServerUnaryReactor* Verify(grpc::CallbackServerContext* context, const auth::VerifyTokenRequest* request, 
        auth::VerifyTokenResponse* response) override;

private:
    DetachedTask DoLoginAsync(const auth::LoginRequest* request, auth::LoginResponse* response, 
        grpc::ServerUnaryReactor* reactor);

    DetachedTask DoRegisterAsync(const auth::RegisterRequest* request, auth::RegisterResponse* response, 
        grpc::ServerUnaryReactor* reactor);

    DetachedTask DoVerifyAsync(const auth::VerifyTokenRequest* request, auth::VerifyTokenResponse* response, 
        grpc::ServerUnaryReactor* reactor);

    MySQLConnPool* mysql_pool_;
    sw::redis::Redis* redis_pool_;

    const char* private_key;
};
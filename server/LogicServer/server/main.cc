#include "LogicServer.h"
#include "../base/threadPool.h"

constexpr std::string dbname            = "chatroom";
constexpr const char* url               = "tcp://127.0.0.1:3306;root;zzc1109aiw";
constexpr int pool_size                 = 2;

int main() {

    MySQLConnPool* pool = MySQLConnPool::getinstance(dbname);
    pool->initpool(url, pool_size);

    sw::redis::ConnectionOptions connection_options;
    connection_options.host = "127.0.0.1";
    connection_options.port = 6379;
    connection_options.db = 1;

    sw::redis::ConnectionPoolOptions pool_options;
    pool_options.size = 2;

    sw::redis::Redis redis(connection_options, pool_options);

    threadpool threadPool{4};

    LogicServer service{pool, &redis, &threadPool};
    ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:5008", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    if(!server) {
        std::cerr << "Failed to start gRPC server! Please check port or configuration." << std::endl;
        exit(1);
    }
    threadPool.start();
    server->Wait();

    return 0;
}
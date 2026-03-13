#include "roomServer.h"

#include <memory>

int main() {

    sw::redis::ConnectionOptions connection_options;
    connection_options.host = "127.0.0.1";
    connection_options.port = 6379;

    sw::redis::ConnectionPoolOptions pool_options;
    pool_options.size = 2;

    sw::redis::Redis redis(connection_options, pool_options);

    RoomServer service{&redis};
    ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:5007", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    if(!server) {
        std::cerr << "Failed to start gRPC server! Please check port or configuration." << std::endl;
        exit(1);
    }
    server->Wait();

    return 0;
}
#pragma once

#include "gateway.grpc.pb.h"
#include "gateway.pb.h"

#include <memory>
#include <functional>

class grpcClient {

public:
    grpcClient();

    void sendSingleMsgAsync(const std::string&, const std::function<void()>&);

private:
    std::shared_ptr<grpc::Channel> GatewayChannel_;
    std::unique_ptr<gateway::GatewayServer::Stub> GatewayStub_;


};
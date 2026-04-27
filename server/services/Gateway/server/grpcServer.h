#pragma once

#include "gateway.grpc.pb.h"
#include "gateway.pb.h"

class GatewayGrpcServer final : public gateway::GatewayServer::CallbackService {

public:
    grpc::ServerUnaryReactor* sendSingleMsg(grpc::CallbackServerContext*, const gateway::sendSingleMsgRequest*, 
        gateway::sendSingleMsgResponse*) override;

private:


};
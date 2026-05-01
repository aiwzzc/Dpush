#pragma once

#include "gateway.grpc.pb.h"
#include "gateway.pb.h"
#include "concurrency/coroutineTask.h"

class GatewayGrpcServer final : public gateway::GatewayServer::CallbackService {

public:
    GatewayGrpcServer();

    grpc::ServerUnaryReactor* sendSingleMsg(grpc::CallbackServerContext*, const gateway::sendSingleMsgRequest*, 
        gateway::sendSingleMsgResponse*) override;

private:
    DetachedTask DosendSingleMsg(grpc::ServerUnaryReactor*, const gateway::sendSingleMsgRequest*, 
        gateway::sendSingleMsgResponse*);

};
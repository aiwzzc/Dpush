#pragma once

#include "gateway.grpc.pb.h"
#include "gateway.pb.h"

#include <memory>
#include <functional>

class LogicDiscovery;

class grpcClient {

public:
    grpcClient(LogicDiscovery* discover);

    void sendSingleMsgAsync(const std::string&, int32_t userid, const std::string&, 
        const std::function<void()>&);

private:
    LogicDiscovery* discover_;

};
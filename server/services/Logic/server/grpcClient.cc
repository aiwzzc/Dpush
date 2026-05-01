#include "grpcClient.h"
#include "LogicDiscovery.h"
#include <grpcpp/grpcpp.h>

grpcClient::grpcClient(LogicDiscovery* discover) : discover_(discover)
{}

void grpcClient::sendSingleMsgAsync(const std::string& gateway_addr, int32_t userid, const std::string& msg, 
    const std::function<void()>& callback) {

    auto gatewayStub = this->discover_->getStub(gateway_addr);
    if(gatewayStub == nullptr) return;

    auto context = std::make_shared<grpc::ClientContext>();
    auto request = std::make_shared<gateway::sendSingleMsgRequest>();
    auto response = std::make_shared<gateway::sendSingleMsgResponse>();

    request->set_user_id(userid);
    request->set_message(std::move(msg));

    gatewayStub->async()->sendSingleMsg(context.get(), request.get(), response.get(), 
    [context, request, response, callback] (grpc::Status s) {
        if(s.ok()) {
            callback();
        }
    });
}
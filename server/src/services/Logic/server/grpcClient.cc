#include "grpcClient.h"
#include <grpcpp/grpcpp.h>

grpcClient::grpcClient(pulse::config::LogicConfig& config, 
    pulse::net::ServiceRegistryClient* serviceRegistryClient): 
    config_(config), 
    serviceRegistryClient_(serviceRegistryClient) {

    this->gateway_stubs_.Init(*this->serviceRegistryClient_, this->config_.serviceRegistry.gateway_prefix_);
}

void grpcClient::sendSingleMsgAsync(const std::string& gateway_endpoint, int32_t userid, const std::string& msg, 
    const std::function<void()>& callback) {

    auto gatewayStub = this->gateway_stubs_.GetStubFromEndpoint(*this->serviceRegistryClient_, gateway_endpoint);
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
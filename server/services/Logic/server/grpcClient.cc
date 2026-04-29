#include "grpcClient.h"
#include <grpcpp/grpcpp.h>

grpcClient::grpcClient() : GatewayChannel_(grpc::CreateChannel("127.0.0.1:5005", grpc::InsecureChannelCredentials())),
GatewayStub_(gateway::GatewayServer::NewStub(this->GatewayChannel_))
{}

void grpcClient::sendSingleMsgAsync(int32_t userid, const std::string& msg, const std::function<void()>& callback) {

    auto context = std::make_shared<grpc::ClientContext>();
    auto request = std::make_shared<gateway::sendSingleMsgRequest>();
    auto response = std::make_shared<gateway::sendSingleMsgResponse>();

    request->set_user_id(userid);
    request->set_message(std::move(msg));

    this->GatewayStub_->async()->sendSingleMsg(context.get(), request.get(), response.get(), 
    [context, request, response, callback] (grpc::Status s) {
        if(s.ok()) {
            callback();
        }
    });
}
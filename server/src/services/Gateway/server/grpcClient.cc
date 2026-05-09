#include "grpcClient.h"

void grpcClient::rpcCilentMessageAsync(const std::string& message, int32_t userid, std::string username,
    std::function<void(std::string)> callback) {

    auto request = std::make_shared<logic::clientMessageRequest>();

    request->set_message(std::move(message));
    request->set_userid(userid);
    request->set_username(username);

    auto response = std::make_shared<logic::clientMessageResponse>();
    auto context = std::make_shared<ClientContext>();

    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context->set_deadline(deadline);

    auto stub = this->logic_stubs_.GetStub(*this->ServiceRegistryClient_, this->logic_prefix_);
    if(stub == nullptr) return;

    stub->async()->clientMessage(context.get(), request.get(), response.get(), 
    [request, response, context, callback] (grpc::Status s) {
        if(s.ok() && !response->message().empty()) {
            callback(response->message());

        } else {
            callback({});
        }
    });
}

void grpcClient::start() {
    this->logic_stubs_.Init(*this->ServiceRegistryClient_, this->logic_prefix_);
    this->room_stubs_.Init(*this->ServiceRegistryClient_, this->room_prefix_);
}

void grpcClient::rpcclearCursorsAsync(int32_t userid, std::function<void()> callback) {

    auto context = std::make_shared<ClientContext>();
    auto request = std::make_shared<logic::clearCursorsRequest>();
    auto response = std::make_shared<logic::clearCursorsResponse>();

    request->set_userid(userid);

    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context->set_deadline(deadline);

    auto stub = this->logic_stubs_.GetStub(*this->ServiceRegistryClient_, this->logic_prefix_);
    if(stub == nullptr) return;

    stub->async()->clearCursors(context.get(), request.get(), response.get(), 
    [context, request, response, callback] (grpc::Status s) {
        if(s.ok()) callback();
    });
}

void grpcClient::rpcGetUserRoomListAsync(int32_t userid, const std::string& addr, 
    const std::function<void(std::vector<std::string>&)>& callback) {

    auto context = std::make_shared<ClientContext>();
    auto request = std::make_shared<room::GetUserRoomListRequest>();
    auto response = std::make_shared<room::GetUserRoomListResponse>();

    request->set_userid(userid);
    request->set_gatewayip(addr);

    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context->set_deadline(deadline);

    auto stub = this->room_stubs_.GetStub(*this->ServiceRegistryClient_, this->room_prefix_);
    if(stub == nullptr) return;

    stub->async()->GetUserRoomList(context.get(), request.get(), response.get(), 
    [request, response, context, callback = std::move(callback)] (grpc::Status s) {
        if(s.ok()) {
            std::vector<std::string> roomlist;
    
            for(const auto& roominfo : response->roomlist()) {
                std::string roomid = roominfo.room_id();

                roomlist.emplace_back(roomid);
            }
        
            callback(roomlist);
        }
    });
}

void grpcClient::rpcJoinRooms(int32_t userid, std::vector<std::string>& rooms) {
    ClientContext ctx;
    room::JoinRoomResponse response;

    auto stub = this->room_stubs_.GetStub(*this->ServiceRegistryClient_, this->room_prefix_);
    if(stub == nullptr) return;

    auto writer = stub->JoinRooms(&ctx, &response);

    for(auto& room : rooms) {
        room::JoinRoomRequest req;
        req.set_userid(userid);
        req.set_room_id(room);

        writer->Write(req);
    }

    writer->WritesDone();
    grpc::Status status = writer->Finish();
}

BathPullClientReactor::BathPullClientReactor(logic::LogicServer::Stub* stub, 
    const std::string& msg,
    const std::function<void(const std::string&)>& onmessage, 
    const std::function<void(const ::grpc::Status&)>& ondone) : 
    on_message_cb_(std::move(onmessage)), on_done_cb_(std::move(ondone)) {

    this->request_.set_message(msg);

    stub->async()->bathPullMessage(&this->context_, &this->request_, this);
    StartRead(&this->response_);
    StartCall();
}

void BathPullClientReactor::OnReadDone(bool ok) {
    if(ok) {
        if(this->on_message_cb_) {
            on_message_cb_(this->response_.message());
        }

        StartRead(&this->response_);

    } else {

    }
}

void BathPullClientReactor::OnDone(const ::grpc::Status& status) {
    if(this->on_done_cb_) {
        this->on_done_cb_(status);
    }

    delete this;
}

void grpcClient::rpcBathPullMessageAsync(const std::string& message, std::function<void(const std::string&)> callback) {

    auto stub = this->logic_stubs_.GetStub(*this->ServiceRegistryClient_, this->logic_prefix_);
    if(stub == nullptr) return;

    new BathPullClientReactor(stub.get(), message, std::move(callback), 
    [] (const ::grpc::Status& status) {
        if(status.ok()) {

        }
    });
}

void grpcClient::rpcIsSubSessionAsync(int32_t userid, std::string& room_id, const std::function<void(const std::string&)>& callback) {
    auto ctx = std::make_shared<ClientContext>();
    auto request = std::make_shared<room::IsSubRoomRequest>();
    auto response = std::make_shared<room::IsSubRoomResponse>();

    request->set_userid(userid);
    request->set_room_id(room_id);

    auto stub = this->room_stubs_.GetStub(*this->ServiceRegistryClient_, this->room_prefix_);
    if(stub == nullptr) return;

    stub->async()->IsSubRoom(ctx.get(), request.get(), response.get(), 
    [ctx, request, response, callback = std::move(callback)] (grpc::Status s) {
        if(s.ok()) {
            callback(response->message());
        }
    });
}

void grpcClient::rpcPullMessageAsync(int64_t roomid, std::string& roomname, const std::function<void(const std::string&)>& callback) {
    auto ctx = std::make_shared<ClientContext>();
    auto request = std::make_shared<logic::PullMessageRequest>();
    auto response = std::make_shared<logic::PullMessageResponse>();

    request->set_roomid(roomid);
    request->set_roomname(roomname);
    request->set_messageid(-1);

    auto stub = this->logic_stubs_.GetStub(*this->ServiceRegistryClient_, this->logic_prefix_);
    if(stub == nullptr) return;

    stub->async()->pullMessage(ctx.get(), request.get(), response.get(),
    [ctx, request, response, callback = std::move(callback)] (grpc::Status s) {
        callback(response->message());
    });
}
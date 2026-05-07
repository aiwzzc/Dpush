#include "GatewayRegister.h"


GatewayRegister::GatewayRegister(const std::string& url, const std::string& ip_port):
etcd_url_(url), ip_port_(ip_port) {

    this->etcd_key_ = "/services/gateway/" + this->ip_port_;
    this->etcd_client_ = std::make_shared<etcd::Client>(this->etcd_url_);
}

void GatewayRegister::start() {
    int ttl = 30;
    auto lease_response = this->etcd_client_->leasegrant(ttl).get();
    int64_t lease_id = lease_response.value().lease();

    this->etcd_client_->put(this->etcd_key_, this->ip_port_, lease_id).get();
    this->etcd_keep_alive_ = std::make_shared<etcd::KeepAlive>(*this->etcd_client_, ttl, lease_id);
}

void GatewayRegister::stop() {
    if(this->etcd_keep_alive_) this->etcd_keep_alive_->Cancel();
    this->etcd_client_->rm(this->etcd_key_).get();
}
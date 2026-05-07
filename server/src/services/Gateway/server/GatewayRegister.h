#pragma once

#include <memory>
#include <string>

#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>

class GatewayRegister {

public:
    GatewayRegister(const std::string& url, const std::string& ip_port);

    void start();
    void stop();

private:
    std::string etcd_url_;
    std::string etcd_key_;
    std::string ip_port_;

    std::shared_ptr<etcd::Client> etcd_client_;
    std::shared_ptr<etcd::KeepAlive> etcd_keep_alive_;

};
#include "AuthServer.h"

AuthServer::AuthServer(const std::string& etcd_url) :
ServiceRegistryClient_(etcd_url) {}

void AuthServer::start() {
    this->ServiceRegistryClient_.RegisterSelf("/services/auth/", "192.168.183.130:5006");
}
#pragma once

#include "etcdServiceNode/ServiceRegistry.h"

class AuthServer {

public:
    AuthServer(const std::string& etcd_url);

    void start();

private:
    pulse::net::ServiceRegistryClient ServiceRegistryClient_;

};
#pragma once

#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>

class ServiceNode {

public:
    void RegisterSelf();
    void DiscoveryOthers();

private:

    
};
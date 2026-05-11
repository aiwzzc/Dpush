#include "netAddress.h"

namespace pulse::net {

NetAddress::NetAddress(const std::string& ip, uint16_t port):
ip_(ip), port_(port) {}

NetAddress NetAddress::AnyAddr(uint16_t port) {
    return NetAddress(kAnyAddr.data(), port);
}

NetAddress NetAddress::LoopbackAddr(uint16_t port) {
    return NetAddress(kLoopback.data(), port);
}

std::string NetAddress::GetUrl() {
    return this->ip_ + ":" + std::to_string(this->port_);
}

};
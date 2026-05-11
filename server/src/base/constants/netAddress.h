#pragma once

#include <cstdint>
#include <string_view>
#include <string>

namespace pulse::net {

inline constexpr std::string_view kAnyAddr = "0.0.0.0";
inline constexpr std::string_view kLoopback = "127.0.0.1";

class NetAddress {

public:
    NetAddress(const std::string& ip, uint16_t port);

    static NetAddress AnyAddr(uint16_t port);
    static NetAddress LoopbackAddr(uint16_t port);

    std::string GetUrl();

private:
    std::string ip_;
    uint16_t port_;

};

};
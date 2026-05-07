#pragma once

#include <string>

class Config {

public:
    static Config& getInstance();

    void Parser(const char* filename);

public:
    std::string instance_id_;
    std::string addr_{"127.0.0.1:5005"};
    int port_{5005};
    std::string listen_addr_{"0.0.0.0:5005"};
    int weight_{1};

};
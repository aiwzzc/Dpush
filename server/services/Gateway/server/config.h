#pragma once

#include <string>

class Config {

public:
    static Config& getInstance();

    void Parser(const char* filename);

public:
    int instance_id_;
    std::string ip_;
    int port_;
    int weight_;

};
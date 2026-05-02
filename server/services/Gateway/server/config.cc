#include "config.h"

#include <iostream>

#include <ryml.hpp>
#include <ryml_std.hpp>
#include <fcntl.h>
 #include <sys/stat.h>
 #include <unistd.h>

static std::string read_file(const char* filename) {
    int fd = ::open(filename, O_RDONLY);
    if(fd < 0) return {};

    struct stat st;
    if(::fstat(fd, &st) < 0) {
        ::close(fd);
        return {};
    }

    std::string buffer;
    buffer.resize(st.st_size);

    int nread = ::read(fd, buffer.data(), st.st_size);
    if(nread <= 0) {
        ::close(fd);
        return {};
    }

    ::close(fd);
    return buffer;
}

Config& Config::getInstance() {
    static Config config;
    return config;
}

void Config::Parser(const char* filename) {
    std::string yaml = read_file(filename);
    if(yaml.empty()) return;

    std::cout << yaml << std::endl;

    ryml::Tree tree = ryml::parse_in_place(ryml::to_substr(yaml));
    ryml::NodeRef root = tree.rootref();

    if (!root.has_child("service")) return;
    
    auto service = root["service"];

    if (service.has_child("instance_id"))
        service["instance_id"] >> this->instance_id_;

    if (service.has_child("host"))
        service["host"] >> this->addr_;

    if (service.has_child("port")) {
        service["port"] >> this->port_;
        this->listen_addr_ = "0.0.0.0:" + std::to_string(this->port_);
    }

    if (service.has_child("weight"))
        service["weight"] >> this->weight_;
}
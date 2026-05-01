#include "config.h"

Config& Config::getInstance() {
    static Config config;
    return config;
}

void Config::Parser(const char* filename) {

}
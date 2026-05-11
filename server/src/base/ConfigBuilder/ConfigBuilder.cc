#include "ConfigBuilder.h"
#include <stdexcept>

namespace pulse::config {

ConfigBuilder::ConfigBuilder(int argc, char* argv[]) {
    this->root_config_path_ = kRootConfigPath;

    for(int i = 1; i < argc; ++i) {
        std::string key = argv[i];

        if(key.starts_with("--")) {
            if(i + 1 >= argc) {
                throw std::runtime_error("missing value for " + key);
            }

            this->args_[key] = argv[++i];
        }
    }

    if(!this->args_.contains("--config")) {
        throw std::runtime_error("missing value for --config");
    }

    this->service_config_path_ = this->args_["--config"];
}

};
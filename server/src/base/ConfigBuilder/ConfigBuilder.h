#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace pulse::config {

inline constexpr std::string_view kRootConfigPath = "../config/root.yaml";

class ConfigBuilder {

public:
    ConfigBuilder(int argc, char* argv[]);

    template<
        typename RootConfigType, 
        typename ServiceConfigType, 
        typename RootConfigParse, 
        typename ServiceConfigParse>
    ServiceConfigType Build(RootConfigParse&& rootParse, ServiceConfigParse&& serviceParse) {
        RootConfigType rootConfig = rootParse(this->root_config_path_);
        ServiceConfigType config = serviceParse(this->service_config_path_, std::move(rootConfig));

        ApplyOverride(config);

        return config;
    }

    template<typename ServiceConfigType>
    void ApplyOverride(ServiceConfigType& config) {
        if(this->args_.contains("--id")) {
            config.service.instance_id = this->args_["--id"];
        }

        if(this->args_.contains("--port")) {
            config.service.port = std::stoi(this->args_["--port"]);
            config.endpoint = config.service.host + ":" + std::to_string(config.service.port);
        }
    }

private:
    std::string root_config_path_;
    std::string service_config_path_;

    std::unordered_map<std::string, std::string> args_;
};

};
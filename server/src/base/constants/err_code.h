#pragma once

#include <string_view>

namespace pulse::constants::err {

    inline constexpr std::string_view KGrpcFailedStartErrorMsg = 
    "Failed to start gRPC server! Please check port or configuration";
    
    inline constexpr std::string_view KKafkaFailedStoreOffsetErrorMsg = "Failed to store offset";

    inline constexpr std::string_view KRedisPipelineExeFailedErrorMsg = "Redis Pipeline execution failed";
};
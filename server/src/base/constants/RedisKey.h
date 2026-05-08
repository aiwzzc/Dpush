#pragma once

#include <string>
#include <string_view>

namespace pulse::constants::rediskey {

inline constexpr std::string_view MessagePersistfield = "payload";
inline constexpr std::string_view GatewayLoadKey = "gateway:load";
inline constexpr std::string_view GatewaySubChannelPrefix = "room:";

class RedisKey {

public:
    static std::string UserRouteGatewayKey(std::string_view uid);
    static std::string GatewaySubRoomChannelKey(std::string_view session_id);
    static std::string MessagePersistKey(std::string_view session_id);
    static std::string MessagePersistMsgId(long long msg_id);
    static std::string UserJoinedSessionKey(int32_t userid);

    static std::string ClientMsgIdKey(std::string_view session_id);
    static std::string MessageSeqIdKey(std::string_view session_id);
};

}; // namespace pulse::constants::rediskey


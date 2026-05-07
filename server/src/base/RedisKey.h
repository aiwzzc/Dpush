#pragma once

#include <string>
#include <string_view>

inline constexpr std::string_view MessagePersistfield = "payload";
inline constexpr std::string_view GatewayLoadKey = "gateway:load";
inline constexpr std::string_view USER_ID = "userid";
inline constexpr std::string_view USERNAME = "username";
inline constexpr std::string_view EXPIRE = "exp";
inline constexpr std::string_view GatewaySubChannelPrefix = "room:";

inline constexpr std::string_view SignalCreateSessionReq = "subscribe_room";
inline constexpr std::string_view SignalCreateSessionRes = "subscribe_ack";
inline constexpr std::string_view SignalJoinSessionReq = "join_room";
inline constexpr std::string_view OK = "ok";
inline constexpr std::string_view FALSE = "false";

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
#include "RedisKey.h"

std::string RedisKey::UserRouteGatewayKey(std::string_view uid){
    return "{route:uid:" + std::string(uid) + "}";
}

std::string RedisKey::GatewaySubRoomChannelKey(std::string_view session_id){
    return "room:" + std::string(session_id);
}

std::string RedisKey::MessagePersistKey(std::string_view session_id){
    return "{" + std::string(session_id) + "}";
}

std::string RedisKey::MessagePersistMsgId(long long msg_id){
    return std::to_string(msg_id) + "-0";
}

std::string RedisKey::UserJoinedSessionKey(int32_t userid) {
    return "{user:rooms:" + std::to_string(userid) + "}";
}

std::string RedisKey::ClientMsgIdKey(std::string_view session_id){
    return "client_msg_set:{" + std::string(session_id) + "}";
}

std::string RedisKey::MessageSeqIdKey(std::string_view session_id){
    return "room_seq:{" + std::string(session_id) + "}";
}
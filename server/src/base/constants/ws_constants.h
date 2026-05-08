#pragma once
#include <string_view>

namespace pulse::constants::ws {

    inline constexpr std::string_view SignalCreateSessionReq = "subscribe_room";
    inline constexpr std::string_view SignalCreateSessionRes = "subscribe_ack";
    inline constexpr std::string_view SignalJoinSessionReq = "join_room";

    inline constexpr std::string_view OK = "ok";
    inline constexpr std::string_view FALSE = "false";
    inline constexpr std::string_view WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    std::string buildHandshakeResponse(std::string_view accept) {
        return
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " +
            std::string(accept) +
            "\r\n\r\n";
    }

};
#pragma once
#include <string_view>

namespace pulse::constants::http {

    // 状态响应报文
    inline constexpr std::string_view RESP_400_BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\n\r\n";
    inline constexpr std::string_view RESP_101_SWITCHING = "HTTP/1.1 101 Switching Protocols\r\n";
    inline constexpr std::string_view RESP_401_UNAUTH = "HTTP/1.1 401 Unauthorized\r\n\r\n";

    // head
    inline constexpr std::string_view HEADER_WS_KEY = "Sec-WebSocket-Key:";
    inline constexpr std::string_view HEADER_WS_UPGRADE = "Upgrade: websocket\r\n";

    // Content-Type
    inline constexpr std::string_view CONTENT_TYPE_OCTET = "application/octet-stream";
    inline constexpr std::string_view CONTENT_TYPE_JSON = "application/json";

    // 状态信息
    inline constexpr std::string_view MSG_OK = "OK";
    inline constexpr std::string_view MSG_BAD_REQUEST = "Bad Request";

    // api path
    inline constexpr std::string_view DISPATCH_API_PATH = "/api/get_gateway";
    inline constexpr std::string_view LOGIN_API_PATH = "/api/login";
    inline constexpr std::string_view REGISTER_API_PATH = "/api/reg";
    inline constexpr std::string_view CREATESESSION_API_PATH = "/api/createsession";
    inline constexpr std::string_view JOINSESSION_API_PATH = "/api/joinsession";

    inline constexpr std::string_view kCRLF = "\r\n";
    inline constexpr std::string_view KCOOKIE = "Cookie";
};
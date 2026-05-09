#pragma once

#include <string>

namespace pulse::protocol::fbs {

struct LoginInfo {
    int errcode{-1};
    std::string errmsg;
    std::string token;
    int32_t userid{0};
    std::string username;
};

struct RegisterInfo {
    int errcode{-1};
    std::string errmsg;
};

struct CreateSessionInfo {
    int errcode{-1};
    std::string errmsg;
    int32_t userid{0};
    int64_t room_id;
};

struct JoinSessionInfo {
    int errcode{-1};
    std::string errmsg;
    int32_t userid{0};
    int64_t room_id;
};

class httpfbsCodec {

public:
    static std::string BuildLoginResfbs(const LoginInfo& info);
    static std::string BuildRegisterResfbs(const RegisterInfo& info);
    static std::string BuildJoinSessionResfbs(const JoinSessionInfo& info);
    static std::string BuildCreateSessionResfbs(const CreateSessionInfo& info);

};



}; // namespace pulse::protocol::fbs

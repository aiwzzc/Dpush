#include "http_codec.h"

#include "chat_generated.h"

namespace pulse::protocol::fbs {

std::string httpfbsCodec::BuildLoginResfbs(const LoginInfo& info) {
    thread_local flatbuffers::FlatBufferBuilder builder(128);
    builder.Clear();

    auto err_msg_offset = builder.CreateString(info.errmsg);
    auto username_offset = builder.CreateString(info.username);
    auto token_offset = builder.CreateString(info.token);

    ChatApp::LoginHttpResbodyPayloadBuilder resBuilder(builder);
    resBuilder.add_code(info.errcode);
    resBuilder.add_err_msg(err_msg_offset);
    resBuilder.add_userid(info.userid);
    resBuilder.add_username(username_offset);
    resBuilder.add_token(token_offset);
    auto loginResOffset = resBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_LoginHttpResbodyPayload);
    rootMsgBuilder.add_payload(loginResOffset.Union());
    auto rootMsgOffset = rootMsgBuilder.Finish();

    builder.Finish(rootMsgOffset);
    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int size = builder.GetSize();

    return std::string(data, size);
}

std::string httpfbsCodec::BuildRegisterResfbs(const RegisterInfo& info) {
    thread_local flatbuffers::FlatBufferBuilder builder(128);
    builder.Clear();

    auto err_msg_offset = builder.CreateString(info.errmsg);

    ChatApp::RegisterHttpResbodyPayloadBuilder resBuilder(builder);
    resBuilder.add_code(info.errcode);
    resBuilder.add_err_msg(err_msg_offset);
    auto resOffset = resBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_RegisterHttpResbodyPayload);
    rootMsgBuilder.add_payload(resOffset.Union());
    auto rootMsgOffset = rootMsgBuilder.Finish();

    builder.Finish(rootMsgOffset);
    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int size = builder.GetSize();

    return std::string(data, size);
}

std::string httpfbsCodec::BuildJoinSessionResfbs(const JoinSessionInfo& info) {
    thread_local flatbuffers::FlatBufferBuilder builder(128);
    builder.Clear();

    auto roomid_offset = builder.CreateString(std::to_string(info.room_id));
    auto err_msg_offset = builder.CreateString(info.errmsg);

    ChatApp::JoinSessionHttpResbodyPayloadBuilder resBuilder(builder);
    resBuilder.add_code(info.errcode);
    resBuilder.add_err_msg(err_msg_offset);
    resBuilder.add_userid(info.userid);
    resBuilder.add_room_id(roomid_offset);
    auto resOffset = resBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_JoinSessionHttpResbodyPayload);
    rootMsgBuilder.add_payload(resOffset.Union());
    auto rootMsgOffset = rootMsgBuilder.Finish();

    builder.Finish(rootMsgOffset);

    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int size = builder.GetSize();

    return std::string(data, size);
}

std::string httpfbsCodec::BuildCreateSessionResfbs(const CreateSessionInfo& info) {
    thread_local flatbuffers::FlatBufferBuilder builder(128);
    builder.Clear();

    auto roomid_offset = builder.CreateString(std::to_string(info.room_id));
    auto err_msg_offset = builder.CreateString(info.errmsg);

    ChatApp::createSessionHttpResbodyPayloadBuilder resBuilder(builder);
    resBuilder.add_code(info.errcode);
    resBuilder.add_err_msg(err_msg_offset);
    resBuilder.add_userid(info.userid);
    resBuilder.add_room_id(roomid_offset);
    auto resOffset = resBuilder.Finish();

    ChatApp::RootMessageBuilder rootMsgBuilder(builder);
    rootMsgBuilder.add_payload_type(ChatApp::AnyPayload_createSessionHttpResbodyPayload);
    rootMsgBuilder.add_payload(resOffset.Union());
    auto rootMsgOffset = rootMsgBuilder.Finish();

    builder.Finish(rootMsgOffset);

    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int size = builder.GetSize();

    return std::string(data, size);
}

}; // namespace pulse::protocol::fbs
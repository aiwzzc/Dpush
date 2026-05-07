#include "websocketSession.h"
#include "GatewayPubSubManager.h"
#include "chat_generated.h"

#include <openssl/sha.h>
#include <sstream>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <vector>

WsSession::WsSession(const TcpConnectionPtr& conn) : 
conn_(conn), state_(WsState::EXPECTING_HTTP_REQUEST) {}

WsSession::~WsSession() {
    if(this->webconnCloseCallback_) this->webconnCloseCallback_();
}

void WsSession::setUserid(int32_t userid) 
{ this->userid_ = userid; }

void WsSession::setUsername(const std::string& username) 
{ this->username_.assign(username); }

void WsSession::setjoinedRooms(const std::unordered_map<std::string, int>& rooms)
{ this->joinedRooms_ = std::move(rooms); }

std::string& WsSession::username() 
{ return this->username_; }

int32_t WsSession::userid() const 
{ return this->userid_; }

EventLoop* WsSession::getLoop() const { 
    auto conn = this->conn_.lock();

    return conn ? conn->getLoop() : nullptr;
}

void WsSession::forceClose() {
    auto conn = this->conn_.lock();

    if(conn) {
        conn->forceClose();
    }
}

void WsSession::setWebconnCloseCallback(const WebconnCloseCallback& cb) 
{ this->webconnCloseCallback_ = std::move(cb); }

void WsSession::setContext(const std::any& context)
{ this->context_ = context; }

const std::any& WsSession::getContext() const 
{ return this->context_; }

std::any* WsSession::getMutableContext()
{ return &this->context_; }

void WsSession::setRoomIndex(const std::string& room_id, int new_index) {
    auto it = this->joinedRooms_.find(room_id);
    if(it == this->joinedRooms_.end()) return;

    this->joinedRooms_[room_id] = new_index;
}

void WsSession::joinRoom(const std::string& room_id, int new_index) {
    auto it = this->joinedRooms_.find(room_id);
    if(it != this->joinedRooms_.end()) return;

    this->joinedRooms_[room_id] = new_index;
}

std::optional<int> WsSession::getRoom_index(const std::string& room_id) const {
    auto it = this->joinedRooms_.find(room_id);
    if(it == this->joinedRooms_.end()) return std::nullopt;

    return it->second;
}

void WsSession::SubscribeSession(const WsSessionPtr& wsSessionPtr, const std::string& room_id) {
    EventLoop* loop = wsSessionPtr->getLoop();

    loop->runInLoop([wsSessionPtr, room_id] () {
        auto& conns = LocalWebsockConnRoomhash[room_id];
        bool needSubscribeToRedisPub = conns.empty() ? true : false;

        wsSessionPtr->joinRoom(room_id, conns.size());
        conns.push_back(wsSessionPtr);

        if(needSubscribeToRedisPub) GatewayPubSubManager::SubscribeRoomSafe(room_id);
    });
}

const std::unordered_map<std::string, int>& WsSession::getjoinedRooms() const
{ return this->joinedRooms_; }

void WsSession::sendCloseFrame(uint16_t code, const std::string reason) {
    size_t payload_len = 2 + reason.size();
    if (payload_len > 125) {
        payload_len = 125;
    }

    std::vector<uint8_t> frame;
    frame.reserve(2 + payload_len);

    // Byte 0: FIN=1 (0x80), Opcode=8 (0x08) -> 0x88
    frame.push_back(0x88);

    // Byte 1: Mask=0, Payload Length
    frame.push_back(static_cast<uint8_t>(payload_len));

    // Byte 2-3: 状态码 (大端序)
    frame.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(code & 0xFF));

    // Byte 4+: Reason
    size_t actual_reason_len = payload_len - 2;
    if (actual_reason_len > 0) {
        const uint8_t* reason_ptr = reinterpret_cast<const uint8_t*>(reason.data());
        frame.insert(frame.end(), reason_ptr, reason_ptr + actual_reason_len);
    }

    // 注意：这里强转回 char* 传给底层的 send

    auto conn = this->conn_.lock();
    if(conn) conn->send(reinterpret_cast<const char*>(frame.data()), frame.size());
}

bool WsSession::connected() {
    auto conn = this->conn_.lock();

    return conn->connected();
}

void WsSession::disconnect() {
    auto conn = this->conn_.lock();

    if(!conn || conn->disconnected()) return;

    sendCloseFrame(1000, "Normal Close");

    if(this->webconnCloseCallback_) this->webconnCloseCallback_();
    else {
        int32_t userid = this->userid_;

        {
            std::lock_guard<std::mutex> lock(GatewayPubSubManager::WebsockConnhashMutex);
            if(GatewayPubSubManager::WebsockConnhash.contains(userid) && 
                GatewayPubSubManager::WebsockConnhash[userid] == shared_from_this()) {
                GatewayPubSubManager::WebsockConnhash.erase(userid);
            }
        }
        // 去除redis路由表
    }
}

void WsSession::sendPongFrame() {}
bool WsSession::isCloseFrame() 
{ return true; }

void WsSession::send(const std::string& msg) {
    auto conn = this->conn_.lock();

    if(conn) conn->send(msg.c_str(), msg.size());
}

void WsSession::send(const char* msg, std::size_t len) {
    auto conn = this->conn_.lock();

    if(conn) conn->send(msg, len);
}

TcpConnectionPtr WsSession::conn() const {
    auto conn = this->conn_.lock();

    return conn ? conn : nullptr;
}

std::vector<std::string> WsSession::onRead(const TcpConnectionPtr& conn, muduo::net::Buffer* buf) {
    if(conn->disconnected()) return {};

    std::vector<std::string> messageList;

    // 使用 while 循环，处理可能存在的多个粘包帧
    while (buf->readableBytes() >= 2) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(buf->peek());
        bool fin = (bytes[0] & 0x80) != 0;
        uint8_t opcode = bytes[0] & 0x0F;
        bool mask = (bytes[1] & 0x80) != 0;
        uint64_t payload_length = bytes[1] & 0x7F;
        size_t offset = 2;

        // 解析扩展长度
        if (payload_length == 126) {
            if (buf->readableBytes() < 4) return {}; // 半包，等待更多数据

            payload_length = (bytes[2] << 8) | bytes[3];
            offset += 2;

        } else if (payload_length == 127) {
            if (buf->readableBytes() < 10) return {}; // 半包，等待更多数据

            payload_length = 0;
            for(int i = 0; i < 8; ++i) {
                payload_length = (payload_length << 8) | bytes[2+i];
            }
            offset += 8;
        }

        // 解析掩码
        if (mask) {
            if (buf->readableBytes() < offset + 4) return {}; // 半包
            offset += 4;
        }

        // 检查整个帧的数据是否已经全部到达 TCP 缓冲区
        if (buf->readableBytes() < offset + payload_length) {
            return {}; // 半包，退出函数，等待 muduo 下一次触发 onRead
        }

        // 此时已经收到一个完整的帧，提取 Payload
        std::string payload_data{buf->peek() + offset, payload_length};
        
        // 解码
        if (mask) {
            const uint8_t* masking_key = bytes + offset - 4;
            for (size_t i = 0; i < payload_length; i++) {
                payload_data[i] ^= masking_key[i % 4];
            }
        }

        buf->retrieve(offset + payload_length);

        flatbuffers::Verifier verifier((const uint8_t*)payload_data.c_str(), payload_data.size());
        if (!ChatApp::VerifyRootMessageBuffer(verifier)) continue;
        // if (!ChatApp::VerifyRootMessageBuffer(verifier)) {
        //     std::cout << "[ERROR] Flatbuffer Verifier failed! payload_length=" << payload_length << std::endl;
        //     continue; 
        // }

        messageList.emplace_back(std::move(payload_data));

    }
    return messageList;
}
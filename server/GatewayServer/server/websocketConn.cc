#include "websocketConn.h"
#include "GatewayPubSubManager.h"

#include <openssl/sha.h>
#include <sstream>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <vector>

// 构造 WebSocket 数据帧
std::string buildWebSocketFrame(const std::string& payload, uint8_t opcode = 0x01) {
    std::string frame;

    frame.push_back(0x80 | (opcode & 0x0F));

    size_t payload_length = payload.size();
    if (payload_length <= 125) {
        frame.push_back(static_cast<uint8_t>(payload_length));
    } else if (payload_length <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payload_length & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<uint8_t>((payload_length >> (8 * i)) & 0xFF));
        }
    }

    frame += payload;

    return frame;
}

WebsocketConn::WebsocketConn(const TcpConnectionPtr& conn) : conn_(conn) {}

WebsocketConn::~WebsocketConn() = default;

void WebsocketConn::setUserid(int32_t userid) { this->userid_ = userid; }

void WebsocketConn::setUsername(const std::string& username) { this->username_.assign(username); }

std::string& WebsocketConn::username() { return this->username_; }

int32_t WebsocketConn::userid() const { return this->userid_; }

void WebsocketConn::setWebconnCloseCallback(const WebconnCloseCallback& cb) { this->webconnCloseCallback_ = std::move(cb); }

void WebsocketConn::sendCloseFrame(uint16_t code, const std::string reason) {
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

    this->conn_->send(reinterpret_cast<const char*>(frame.data()), frame.size());
}

bool WebsocketConn::connected() { return this->conn_->connected(); }

void WebsocketConn::disconnect() {
    if(this->conn_->disconnected()) return;

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

void WebsocketConn::setgrpcClientPtr(grpcClientPtr client) { this->grpcClient_ = client; }

void WebsocketConn::sendPongFrame() {}
bool WebsocketConn::isCloseFrame() { return true; }

void WebsocketConn::send(const std::string& msg) { this->conn_->send(msg); }
void WebsocketConn::send(const char* msg, std::size_t len) { this->conn_->send(msg, len); }

std::string WebsocketConn::onRead(const TcpConnectionPtr& conn, std::string& buf) {
    if(conn->disconnected()) return "";

    // 使用 while 循环，处理可能存在的多个粘包帧
    while (buf.size() >= 2) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(buf.data());
        bool fin = (bytes[0] & 0x80) != 0;
        uint8_t opcode = bytes[0] & 0x0F;
        bool mask = (bytes[1] & 0x80) != 0;
        uint64_t payload_length = bytes[1] & 0x7F;
        size_t offset = 2;

        // 解析扩展长度
        if (payload_length == 126) {
            if (buf.size() < 4) return ""; // 半包，等待更多数据

            payload_length = (bytes[2] << 8) | bytes[3];
            offset += 2;

        } else if (payload_length == 127) {
            if (buf.size() < 10) return ""; // 半包，等待更多数据

            payload_length = 0;
            for(int i = 0; i < 8; ++i) {
                payload_length = (payload_length << 8) | bytes[2+i];
            }
            offset += 8;
        }

        // 解析掩码
        if (mask) {
            if (buf.size() < offset + 4) return ""; // 半包
            offset += 4;
        }

        // 检查整个帧的数据是否已经全部到达 TCP 缓冲区
        if (buf.size() < offset + payload_length) {
            return ""; // 半包，退出函数，等待 muduo 下一次触发 onRead
        }

        // 此时已经收到一个完整的帧，提取 Payload
        std::string payload_data = buf.substr(offset, payload_length);
        
        // 解码
        if (mask) {
            const uint8_t* masking_key = bytes + offset - 4;
            for (size_t i = 0; i < payload_length; i++) {
                payload_data[i] ^= masking_key[i % 4];
            }
        }

        buf.erase(0, offset + payload_length);

        if (opcode == 0x01) { // 文本帧
            Json::Value root;
            Json::Reader reader;

            if(reader.parse(payload_data, root) && !root["type"].isNull()) {
                std::string type = root["type"].asString();
                if(type == "ClientMessage") {
                    // handleClientMessage(redis, root);
                    return payload_data;

                } else if(type == "RequestRoomHistory" || type == "PullMissingMessages") {
                    std::string message = this->grpcClient_->rpcCilentMessage(payload_data, this->userid_, this->username_);

                    if(!message.empty()) this->send(message);

                    return "";

                } else if(type == "clientCreateRoom") {
                    // handleClientCreateRoom(pool, root);
                    return "";
                }
            }
        } else if (opcode == 0x08) {
            // 处理 Close 帧
            disconnect();
            return "";
        }

        return "";

        // 处理业务逻辑
        // if (opcode == 0x01) { // 文本帧

        //     // std::string message = this->grpcClient_->rpcCilentMessage(payload_data, this->userid_, this->username_);

        //     // if(!message.empty()) this->send(message);

        // } else if (opcode == 0x08) {
        //     // 处理 Close 帧
        //     disconnect();
        // }
    }

    return "";
}
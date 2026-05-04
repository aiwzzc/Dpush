#include "wsServer.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/Buffer.h"
#include "websocketSession.h"

#include "GatewayPubSubManager.h"
#include "GatewayServer.h"
#include "base64.h"
#include "chat_generated.h"
#include "heartbeatManager.h"

#include "grpcClient.h"
#include "producer.h"
#include "config.h"

#include <openssl/sha.h>
#include <jwt.h>

using EventLoop = muduo::net::EventLoop;
using TcpServer = muduo::net::TcpServer;
using TcpConnectionPtr = muduo::net::TcpConnectionPtr;
using Buffer = muduo::net::Buffer;

wsServer::wsServer(const muduo::net::InetAddress &addr, const std::string& name, 
    int num_event_loops, const WsServerContext& ctx): loop_(std::make_unique<EventLoop>()), 
    tcpServer_(std::make_unique<TcpServer>(this->loop_.get(), addr, name, TcpServer::kReusePort)), 
    wsContext_(std::move(ctx)) {

    this->tcpServer_->setMessageCallback([this] (const TcpConnectionPtr& conn, Buffer* buffer, muduo::Timestamp) {
        this->onMessage(conn, buffer);
    });

    this->tcpServer_->setThreadNum(num_event_loops);

    this->tcpServer_->setConnectionCallback([this] (const TcpConnectionPtr& conn) {
        this->onConnection(conn);
    });

    this->tcpServer_->setThreadInitCallback([] (EventLoop* loop) {
        GatewayPubSubManager::RegisterLoop(loop);
    });
}

wsServer::~wsServer() = default;

void wsServer::setThreadInitCallback(const ThreadInitCallback& cb) {
    this->tcpServer_->setThreadInitCallback(cb);
}

void wsServer::setMainLoopTimerCallback(const MainLoopTimerCallback& cb) {
    this->mainLoopTimerCallback_ = std::move(cb);
}

void wsServer::start() {
    this->tcpServer_->start();

    this->loop_->runEvery(3, [this] () {
        if(this->mainLoopTimerCallback_) {
            this->mainLoopTimerCallback_();
        }
    });

    this->loop_->loop(1000);
}

void wsServer::onUpgrade(const TcpConnectionPtr& conn) {
    auto session = *std::any_cast<std::shared_ptr<WsSession>>(conn->getMutableContext());

    {
        std::unique_lock<std::shared_mutex> lock(GatewayServer::user_Eventloop_mutex_);
        GatewayServer::user_Eventloop_[session->userid()] = session->getLoop();
    }

    GatewayServer::conned_count_.fetch_add(1, std::memory_order_relaxed);

    this->wsContext_.grpcClient_->rpcclearCursorsAsync(session->userid(), [] () { return; });

    this->wsContext_.grpcClient_->rpcGetUserRoomListAsync(session->userid(), Config::getInstance().addr_, 
    [session] (std::vector<std::string>& roomlist) {

        if(roomlist.empty()) return;

        EventLoop* loop = session->getLoop();

        loop->runInLoop([session, roomlist = std::move(roomlist)] () {
            for(const auto& roomid : roomlist) {
                WsSession::SubscribeSession(session, roomid);
            }
        });
    });

    session->setWebconnCloseCallback([session] () {
        if(!session->connected()) return;

        int32_t userid = session->userid();

        {
            std::unique_lock<std::shared_mutex> lock(GatewayServer::user_Eventloop_mutex_);
            auto it = GatewayServer::user_Eventloop_.find(userid);
            if(it != GatewayServer::user_Eventloop_.end()) GatewayServer::user_Eventloop_.erase(it);
        }

        EventLoop* loop = session->getLoop();

        loop->runInLoop([userid, session] () {
            if(LocalWebsockConnhash.contains(userid) && LocalWebsockConnhash[userid] == session) {
                LocalWebsockConnhash.erase(userid);
            }

            const std::unordered_map<std::string, int>& myRooms = session->getjoinedRooms();
            for(const auto& [roomid, room_index] : myRooms) {
                auto it = LocalWebsockConnRoomhash.find(roomid);
                if(it != LocalWebsockConnRoomhash.end()) {
                    auto& conns = it->second;

                    if(room_index >= 0 && room_index < conns.size()) {
                        conns[room_index] = conns.back();
                        conns[room_index]->setRoomIndex(roomid, room_index);
                        conns.pop_back();
                        session->setRoomIndex(roomid, -1);
                    }

                    if(conns.empty()) {
                        LocalWebsockConnRoomhash.erase(it);
                        GatewayPubSubManager::UnSubscribeRoomSafe(roomid);
                    }
                }
            }
        });
    });

    EventLoop* loop = session->getLoop();
    loop->runInLoop([session] () {
        LocalWebsockConnhash[session->userid()] = session;
    });
}

extern thread_local std::unique_ptr<heartbeatManager> t_heartbeatManager_ptr;

void wsServer::dispatchEvent(const TcpConnectionPtr& conn, const std::vector<std::string>& messageList) {
    auto session = *std::any_cast<std::shared_ptr<WsSession>>(conn->getMutableContext());

    for(const auto& message : messageList) {
        auto rootMsg = ChatApp::GetRootMessage(message.data());

        switch(rootMsg->payload_type()) {
            case ChatApp::AnyPayload_ClientMessagePayload: {
                auto payload = rootMsg->payload_as_ClientMessagePayload();
                std::string kafka_key, target_id_str{payload->target_id()->c_str(), payload->target_id()->size()};

                if(payload->chat_type() == ChatApp::ChatType::ChatType_Group) {
                    kafka_key = std::move(target_id_str);

                } else if(payload->chat_type() == ChatApp::ChatType::ChatType_Single) {
                    long target_user_id;
                    auto [p, ec] = std::from_chars(target_id_str.data(), 
                    target_id_str.data() + target_id_str.size(), target_user_id);

                    if(ec != std::errc() || target_user_id < 0) return;

                    long own_user_id = session->userid();

                    long maxId = std::max(target_user_id, own_user_id);
                    long minId = std::min(target_user_id, own_user_id);

                    kafka_key = std::to_string(minId) + "-" + std::to_string(maxId);
                }

                std::string_view clientMessageId{payload->client_message_id()->c_str(), 
                    payload->client_message_id()->size()};

                KafkaDeliveryContext* ctx = new KafkaDeliveryContext{conn, clientMessageId.data()};

                this->wsContext_.producer_->produce(kafkaProducer::CLIENTMESSAGETOPIC, message.data(), message.size(), kafka_key.data(), 
                kafka_key.size(), ctx, session->userid(), session->username());

                break;
            }

            case ChatApp::AnyPayload_RequestRoomHistoryPayload: {
                this->wsContext_.grpcClient_->rpcCilentMessageAsync(message, session->userid(), session->username(), 
                [session] (std::string msg) {
                    session->send(buildWebSocketFrame(msg, 0x02));
                });

                break;
            }

            case ChatApp::AnyPayload_PullMissingMessagePayload: {
                this->wsContext_.grpcClient_->rpcCilentMessageAsync(message, session->userid(), session->username(), 
                [session] (std::string msg) {
                    session->send(buildWebSocketFrame(msg, 0x02));
                });

                break;
            }

            case ChatApp::AnyPayload_BatchPullMessagePayload: {
                auto payload = rootMsg->payload_as_BatchPullMessagePayload();
                auto rooms = payload->rooms();

                if(rooms->size() == 0) {
                    session->send(buildWebSocketFrame("", 0x02));
                    break;
                }

                this->wsContext_.grpcClient_->rpcBathPullMessageAsync(message, [session] (const std::string& pulled_msg) {
                    std::cout << pulled_msg.size() << std::endl;
                    session->send(buildWebSocketFrame(pulled_msg, 0x02));
                });

                break;
            }

            case ChatApp::AnyPayload_SignalingFromClientPayload: {
                auto payload = rootMsg->payload_as_SignalingFromClientPayload();
                std::string_view action_type_str(payload->action()->c_str(), payload->action()->size());
                if(action_type_str == "subscribe_room") {
                    std::string room_id(payload->room_id()->c_str(), payload->room_id()->size());

                    this->wsContext_.grpcClient_->rpcIsSubSessionAsync(session->userid(), room_id, 
                    [session, room_id] (const std::string& message) {
                        session->send(buildWebSocketFrame(message, 0x02));

                        WsSession::SubscribeSession(session, room_id);
                    });
                }

                break;
            }

            case ChatApp::AnyPayload_SignalingFromClientJoinPayload: {
                auto payload = rootMsg->payload_as_SignalingFromClientJoinPayload();
                std::string_view action_type_str(payload->action()->c_str(), payload->action()->size());
                if(action_type_str == "join_room") {
                    std::string room_id(payload->room_id()->c_str(), payload->room_id()->size());
                    int64_t roomid = std::stol(room_id);
                    std::string room_name(payload->room_name()->c_str(), payload->room_name()->size());

                    this->wsContext_.grpcClient_->rpcPullMessageAsync(roomid, room_name, 
                    [session, room_id] (const std::string& message) {
                        session->send(buildWebSocketFrame(message, 0x02));

                        WsSession::SubscribeSession(session, room_id);
                    });
                }

                break;
            }

            case ChatApp::AnyPayload_PingPayload: {
                auto payload = rootMsg->payload_as_PingPayload();
                int64_t client_time = payload->ts();

                t_heartbeatManager_ptr->onMessagePing(session, client_time, 
                [session] (const std::string& pong) {
                    session->send(buildWebSocketFrame(pong, 0x02));
                });

                break;
            }
        }
    }
}

void wsServer::onConnection(const muduo::net::TcpConnectionPtr& conn) {
    if(conn->connected()) {
        auto session = std::make_shared<WsSession>(conn);
        conn->setContext(session);
    }
}

static std::string AnalysisCookie(const std::string& header) {
    std::size_t cookie_start = header.find("Cookie");
    if(cookie_start == std::string::npos) return {};

    std::size_t cookie_end = header.find("\r\n", cookie_start);
    if(cookie_start == std::string::npos) return {};

    std::string cookieFields = header.substr(cookie_start, cookie_end - cookie_start);

    std::string target = "sid=";
    std::size_t pos = cookieFields.find(target);
    if (pos == std::string::npos) return {};

    // 找到 sid= 之后的内容，直到遇到分号或字符串结束
    std::size_t start = pos + target.length();
    std::size_t end = cookieFields.find(';', start);
    
    if (end == std::string::npos) {
        return cookieFields.substr(start);
    }
    return cookieFields.substr(start, end - start);
}

static std::string HandleUpgradeResponse(const std::string& header) {
    std::size_t pos = header.find("Sec-WebSocket-Key");
    if(pos == std::string::npos) return "HTTP/1.1 400 Bad Request\r\n\r\n";

    std::size_t end = header.find("\r\n", pos);
    if(end == std::string::npos) return "HTTP/1.1 400 Bad Request\r\n\r\n";

    std::string cli_websocket_key = header.substr(pos, end - pos);
    if(cli_websocket_key.empty()) return "HTTP/1.1 400 Bad Request\r\n\r\n";
    
    cli_websocket_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    unsigned char sha1[20]; //openssl的库
    SHA1(reinterpret_cast<const unsigned char*>(cli_websocket_key.data()), cli_websocket_key.size(), sha1);

    std::string accept = base64_encode(sha1, 20); //base 64编码
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";

    return response;
}

bool wsServer::processHandshake(const std::string& header, const muduo::net::TcpConnectionPtr& conn) {
    if (header.find("GET") != 0) return false;

    if (header.find("Upgrade: websocket") == std::string::npos && 
        header.find("Upgrade: WebSocket") == std::string::npos) {
        return false;
    }

    std::string cookie = AnalysisCookie(header);
    if(cookie.empty()) return false;

    jwt* decoded = nullptr;

    if(jwt_decode(&decoded, cookie.c_str(), (unsigned char*)GatewayServer::public_key, 
    strlen(GatewayServer::public_key)) != 0) return false;

    conn->send(HandleUpgradeResponse(header));

    int32_t userid = jwt_get_grant_int(decoded, "userid");
    const char* uname_ptr = jwt_get_grant(decoded, "username");
    std::string username = uname_ptr ? uname_ptr : "";
    
    jwt_free(decoded);
    if(username.empty()) return false;

    auto session = *std::any_cast<std::shared_ptr<WsSession>>(conn->getMutableContext());
    session->setUserid(userid);
    session->setUsername(username);

    return true;
}

void wsServer::onMessage(const TcpConnectionPtr& conn, Buffer* buffer) {
    auto session = *std::any_cast<std::shared_ptr<WsSession>>(conn->getMutableContext());

    if(session->state_ == WsState::EXPECTING_HTTP_REQUEST) {
        const char* crlf = buffer->find(k2CRLF, 4);

        if(crlf) {
            std::string header_str(buffer->peek(), crlf - buffer->peek() + 4);

            if(this->processHandshake(header_str, conn)) {
                session->state_ = WsState::ESTABLISHED;

                buffer->retrieveUntil(crlf + 4);
                this->onUpgrade(conn);

                if(buffer->readableBytes() == 0) return;

            } else {
                conn->send("HTTP/1.1 401 Unauthorized\r\n\r\n");
                conn->forceClose();

                return;
            }

        } else {
            // 等待下一次onMessage
            return;
        }
    }

    if(session->state_ == WsState::ESTABLISHED) {
        std::vector<std::string> messageList = session->onRead(conn, buffer);

        if(messageList.empty()) return;

        this->dispatchEvent(conn, messageList);
    }
}

#include "handleUpgradeEvent.h"
#include "HttpRequest.h"
#include "base64.h"
#include "websocketConn.h"
#include "GatewayPubSubManager.h"
#include "producer.h"
#include "GatewayServer.h"
#include "chat_generated.h"
#include "heartbeatManager.h"

#include <openssl/sha.h>
#include <jwt.h>
#include <charconv>

extern thread_local std::unique_ptr<heartbeatManager> t_heartbeatManager_ptr;

// std::string AnalysisCookie(const HttpRequest& req) {
//     if(!req.headers().contains("Cookie")) return "";
//     const std::string& cookieFields = *(req.getHeader("Cookie").value());

//     std::stringstream ss(cookieFields);
//     std::string item;
//     std::string sid;

//     while(std::getline(ss, item, ';')) {
//         if(!item.empty() && item[0] == ' ') item.erase(0, 1);

//         if(item.find("sid=") == 0) {
//             sid = item.substr(4);

//             break;
//         }
//     }

//     return sid;
// }

std::string AnalysisCookie(const HttpRequest& req) {
    if(!req.headers().contains("Cookie")) return "";
    const std::string& cookieFields = *(req.getHeader("Cookie").value());

    std::string target = "sid=";
    size_t pos = cookieFields.find(target);
    if (pos == std::string::npos) return "";

    // 找到 sid= 之后的内容，直到遇到分号或字符串结束
    size_t start = pos + target.length();
    size_t end = cookieFields.find(';', start);
    
    if (end == std::string::npos) {
        return cookieFields.substr(start);
    }
    return cookieFields.substr(start, end - start);
}

std::string HandleUpgradeResponse(const HttpRequest& req) {
    std::string cli_websocket_key = *(req.getHeader("Sec-WebSocket-Key").value());
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

void sendbadResponse(const TcpConnectionPtr& conn) {
    std::string rejectRes = "HTTP/1.1 401 Unauthorized\r\n"
                            "Connection: close\r\n"
                            "Content-Length: 0\r\n\r\n";

    conn->send(rejectRes);
    conn->shutdown();
}

void handleUpgradeEvent(const TcpConnectionPtr& conn, const HttpRequest& req, const grpcClientPtr& client,
    const kafkaProducerPtr& producer) {

    if(conn->disconnected()) return;

    if(*(req.getHeader("Upgrade").value()) == "websocket") {
        std::string cookie = AnalysisCookie(req);

        // std::cout << "DEBUG: Raw Cookie String: [" << cookie << "]" << std::endl; 

        jwt* decoded = nullptr;
        bool benchmark{false};

        if(jwt_decode(&decoded, cookie.c_str(), (unsigned char*)GatewayServer::public_key, 
        strlen(GatewayServer::public_key)) != 0) {
            // std::cout << "DEBUG: JWT Decode Failed! Cookie was: " << cookie << std::endl;
            benchmark = true;
            // sendbadResponse(conn);

            // return;
        }

        int32_t userid;
        std::string username;

        if(!benchmark) {
            conn->send(HandleUpgradeResponse(req));
            userid = jwt_get_grant_int(decoded, "userid");
            const char* uname_ptr = jwt_get_grant(decoded, "username");
            username = uname_ptr ? uname_ptr : "";
            
        } else {
            // std::cout << req.path() << std::endl;

            const std::string& req_path = req.path();

            auto [p, ec] = std::from_chars(req_path.data(), req_path.data() + req_path.size(), userid);
            if(ec != std::errc() || userid < 0) return;

            username = "user_" + std::to_string(userid - 32);
        }
        
        jwt_free(decoded);

        if(username.empty()) return;

        auto wsContextPtr = std::make_shared<WebsocketConn>(conn);
        wsContextPtr->setUserid(userid);
        wsContextPtr->setUsername(username);

        {
            std::unique_lock<std::shared_mutex> lock(GatewayServer::user_Eventloop_mutex_);
            GatewayServer::user_Eventloop_[wsContextPtr->userid()] = wsContextPtr->getLoop();
        }

        client->rpcclearCursorsAsync(userid, [] () { return; });

        client->rpcGetUserRoomListAsync(userid, [userid, wsContextPtr, client] (std::vector<std::string>& roomlist) {

            if(roomlist.empty()) return;

#if 0
            for(const auto& roomid : roomlist) {

                bool needSubscribeToRedisPub = false;

                {
                    std::size_t bucketIndex = std::hash<std::string>{}(roomid) % BUCKET_NUM;
                    auto& bucket = GatewayPubSubManager::roomBuckets[bucketIndex];

                    std::lock_guard<std::mutex> lock(bucket.mtx_);

                    auto& conns = bucket.roomHash_[roomid];
                    if(conns.empty()) needSubscribeToRedisPub = true;

                    conns.insert(wsContextPtr);
                }

                if(needSubscribeToRedisPub) GatewayPubSubManager::SubscribeRoomSafe(roomid);

            }
#elif 1

            EventLoop* loop = wsContextPtr->getLoop();

            loop->runInLoop([wsContextPtr, roomlist = std::move(roomlist)] () {
                for(const auto& roomid : roomlist) {
                    WebsocketConn::SubscribeSession(wsContextPtr, roomid);
                }
            });
#endif

        });

        wsContextPtr->setWebconnCloseCallback([wsContextPtr] () {
            if(!wsContextPtr->connected()) return;

            int32_t userid = wsContextPtr->userid();

            {
                std::unique_lock<std::shared_mutex> lock(GatewayServer::user_Eventloop_mutex_);
                auto it = GatewayServer::user_Eventloop_.find(userid);
                if(it != GatewayServer::user_Eventloop_.end()) GatewayServer::user_Eventloop_.erase(it);
            }

#if 0
            {
                std::lock_guard<std::mutex> lock(GatewayPubSubManager::WebsockConnhashMutex);
                if(GatewayPubSubManager::WebsockConnhash.contains(userid) && 
                    GatewayPubSubManager::WebsockConnhash[userid] == wsContextPtr) {
                    GatewayPubSubManager::WebsockConnhash.erase(userid);
                }
                // 还需要去redis删除对应的路由表(分布式)
            }

            for(auto& bucket : GatewayPubSubManager::roomBuckets) {
                {
                    std::lock_guard<std::mutex> lock(bucket.mtx_);
                    for(auto it = bucket.roomHash_.begin(); it != bucket.roomHash_.end(); ++it) {
                        auto conn = it->second.find(wsContextPtr);

                        if(conn != it->second.end()) it->second.erase(wsContextPtr);
                    }
                }
            }
#elif 1
            EventLoop* loop = wsContextPtr->getLoop();

            loop->runInLoop([userid, wsContextPtr] () {
                if(LocalWebsockConnhash.contains(userid) && LocalWebsockConnhash[userid] == wsContextPtr) {
                    LocalWebsockConnhash.erase(userid);
                }

                const std::unordered_map<std::string, int>& myRooms = wsContextPtr->getjoinedRooms();
                for(const auto& [roomid, room_index] : myRooms) {
                    auto it = LocalWebsockConnRoomhash.find(roomid);
                    if(it != LocalWebsockConnRoomhash.end()) {
                        auto& conns = it->second;

                        if(room_index >= 0 && room_index < conns.size()) {
                            conns[room_index] = conns.back();
                            conns[room_index]->setRoomIndex(roomid, room_index);
                            conns.pop_back();
                            wsContextPtr->setRoomIndex(roomid, -1);
                        }

                        if(conns.empty()) {
                            LocalWebsockConnRoomhash.erase(it);
                            GatewayPubSubManager::UnSubscribeRoomSafe(roomid);
                        }
                    }
                }
            });
#endif
        });

        conn->setContext(wsContextPtr);

#if 0
        {
            std::lock_guard<std::mutex> lock(GatewayPubSubManager::WebsockConnhashMutex);
            GatewayPubSubManager::WebsockConnhash[userid] = wsContextPtr;
        }
#elif 1
        EventLoop* loop = wsContextPtr->getLoop();
        loop->runInLoop([userid, wsContextPtr] () {
            LocalWebsockConnhash[userid] = wsContextPtr;
        });
#endif

        conn->setMessageCallback([producer, client] (const TcpConnectionPtr& conn, muduo::net::Buffer* buffer, muduo::Timestamp) {
            if(conn->disconnected()) return;
            WebsocketConnPtr wsContextPtr = *(std::any_cast<WebsocketConnPtr>(conn->getMutableContext()));

            std::vector<std::string> messageList = wsContextPtr->onRead(conn, buffer);

            if(messageList.empty()) return;

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

                            long own_user_id = wsContextPtr->userid();

                            long maxId = std::max(target_user_id, own_user_id);
                            long minId = std::min(target_user_id, own_user_id);

                            kafka_key = std::to_string(minId) + "-" + std::to_string(maxId);
                        }

                        std::string_view clientMessageId{payload->client_message_id()->c_str(), 
                            payload->client_message_id()->size()};

                        KafkaDeliveryContext* ctx = new KafkaDeliveryContext{conn, clientMessageId.data()};

                        producer->produce(kafkaProducer::CLIENTMESSAGETOPIC, message.data(), message.size(), kafka_key.data(), 
                        kafka_key.size(), ctx, wsContextPtr->userid(), wsContextPtr->username());

                        break;
                    }

                    case ChatApp::AnyPayload_RequestRoomHistoryPayload: {
                        client->rpcCilentMessageAsync(message, wsContextPtr->userid(), wsContextPtr->username(), 
                        [wsContextPtr] (std::string msg) {
                            wsContextPtr->send(buildWebSocketFrame(msg, 0x02));
                        });

                        break;
                    }

                    case ChatApp::AnyPayload_PullMissingMessagePayload: {
                        client->rpcCilentMessageAsync(message, wsContextPtr->userid(), wsContextPtr->username(), 
                        [wsContextPtr] (std::string msg) {
                            wsContextPtr->send(buildWebSocketFrame(msg, 0x02));
                        });

                        break;
                    }

                    case ChatApp::AnyPayload_BatchPullMessagePayload: {
                        auto payload = rootMsg->payload_as_BatchPullMessagePayload();
                        auto rooms = payload->rooms();

                        if(rooms->size() == 0) {
                            wsContextPtr->send(buildWebSocketFrame("", 0x02));
                            break;
                        }

                        client->rpcBathPullMessageAsync(message, [wsContextPtr] (const std::string& pulled_msg) {
                            wsContextPtr->send(buildWebSocketFrame(pulled_msg, 0x02));
                        });

                        break;
                    }

                    case ChatApp::AnyPayload_SignalingFromClientPayload: {
                        auto payload = rootMsg->payload_as_SignalingFromClientPayload();
                        std::string_view action_type_str(payload->action()->c_str(), payload->action()->size());
                        if(action_type_str == "subscribe_room") {
                            std::string room_id(payload->room_id()->c_str(), payload->room_id()->size());

                            client->rpcIsSubSessionAsync(wsContextPtr->userid(), room_id, 
                            [wsContextPtr, room_id] (const std::string& message) {
                                wsContextPtr->send(buildWebSocketFrame(message, 0x02));

                                WebsocketConn::SubscribeSession(wsContextPtr, room_id);
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

                            client->rpcPullMessageAsync(roomid, room_name, 
                            [wsContextPtr, room_id] (const std::string& message) {
                                wsContextPtr->send(buildWebSocketFrame(message, 0x02));

                                WebsocketConn::SubscribeSession(wsContextPtr, room_id);
                            });
                        }

                        break;
                    }

                    case ChatApp::AnyPayload_PingPayload: {
                        auto payload = rootMsg->payload_as_PingPayload();
                        int64_t client_time = payload->ts();

                        t_heartbeatManager_ptr->onMessagePing(wsContextPtr, client_time, 
                        [wsContextPtr] (const std::string& pong) {
                            wsContextPtr->send(buildWebSocketFrame(pong, 0x02));
                        });

                        break;
                    }
                }
            }
        });
    }
}
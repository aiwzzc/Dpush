#include "handleUpgradeEvent.h"
#include "httpServer/HttpRequest.h"
#include "base/base64.h"
#include "websocketConn.h"
#include "GatewayPubSubManager.h"
#include "producer.h"
#include "GatewayServer.h"
#include "../../flatbuffers/chat_generated.h"

#include <openssl/sha.h>
#include <jwt.h>

std::string AnalysisCookie(const HttpRequest& req) {
    if(!req.headers().contains("Cookie")) return "";
    const std::string& cookieFields = *(req.getHeader("Cookie").value());

    std::stringstream ss(cookieFields);
    std::string item;
    std::string sid;

    while(std::getline(ss, item, ';')) {
        if(!item.empty() && item[0] == ' ') item.erase(0, 1);

        if(item.find("sid=") == 0) {
            sid = item.substr(4);

            break;
        }
    }

    return sid;
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

        jwt* decoded = nullptr;
        bool benchmark{false};

        if(jwt_decode(&decoded, cookie.c_str(), (unsigned char*)GatewayServer::public_key, 
        strlen(GatewayServer::public_key)) != 0) {
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
            userid = std::stoi(req.path());
            username = "user_" + std::to_string(userid - 32);
        }
        
        jwt_free(decoded);

        if(username.empty()) return;

        auto wsContextPtr = std::make_shared<WebsocketConn>(conn);
        wsContextPtr->setUserid(userid);
        wsContextPtr->setUsername(username);

        client->rpcclearCursorsAsync(userid, [] () { return; });

        auto roomlist = GatewayPubSubManager::UserRoomLRU_.get(userid);

        if(roomlist.has_value()) {
            EventLoop* loop = wsContextPtr->getLoop();

            loop->runInLoop([wsContextPtr, roomlist = std::move(roomlist)] () {
                for(const auto& roomid : roomlist.value()) {
                    bool needSubscribeToRedisPub = false;

                    auto& conns = LocalWebsockConnRoomhash[roomid];
                    if(conns.empty()) needSubscribeToRedisPub = true;

                    wsContextPtr->room_index_ = conns.size();
                    conns.push_back(wsContextPtr);
                    if(needSubscribeToRedisPub) GatewayPubSubManager::SubscribeRoomSafe(roomid);

                    wsContextPtr->addRoom(roomid);
                }
            });

        } else {

            client->rpcGetUserRoomListAsync(userid, [userid, wsContextPtr, client] (std::vector<std::string> roomlist) {

                if(roomlist.empty()) return;

                GatewayPubSubManager::UserRoomLRU_.put(userid, roomlist);

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
                        bool needSubscribeToRedisPub = false;

                        auto& conns = LocalWebsockConnRoomhash[roomid];
                        if(conns.empty()) needSubscribeToRedisPub = true;

                        wsContextPtr->room_index_ = conns.size();
                        conns.push_back(wsContextPtr);
                        if(needSubscribeToRedisPub) GatewayPubSubManager::SubscribeRoomSafe(roomid);

                        wsContextPtr->addRoom(roomid);
                    }
                });
#endif
    
            });
        }

        // client->rpcinitialPullMessageAsync(wsContextPtr->userid(), wsContextPtr->username(), 10, 
        // [wsContextPtr] (std::string msg) { wsContextPtr->send(buildWebSocketFrame(msg, 0x02)); });

        wsContextPtr->setWebconnCloseCallback([wsContextPtr] () {
            if(!wsContextPtr->connected()) return;

            int32_t userid = wsContextPtr->userid();

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

                std::vector<std::string> myRooms = wsContextPtr->getjoinedRooms();
                for(const auto& roomid : myRooms) {
                    auto it = LocalWebsockConnRoomhash.find(roomid);
                    auto& conns = it->second;

                    if(it != LocalWebsockConnRoomhash.end()) {
                        int room_index = wsContextPtr->room_index_;

                        if(room_index >= 0 && room_index < conns.size()) {
                            conns[room_index] = conns.back();
                            conns[room_index]->room_index_ = room_index;
                            conns.pop_back();
                            wsContextPtr->room_index_ = -1;
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

                        std::string_view roomid{payload->room_id()->c_str(), payload->room_id()->size()};
                        std::string_view clientMessageId{payload->client_message_id()->c_str(), 
                            payload->client_message_id()->size()};

                        KafkaDeliveryContext* ctx = new KafkaDeliveryContext{conn, clientMessageId.data()};

                        producer->produce(kafkaProducer::CLIENTMESSAGETOPIC, message.data(), message.size(), roomid.data(), 
                        roomid.size(), ctx, wsContextPtr->userid(), wsContextPtr->username());

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
                }
            }
        });
    }
}
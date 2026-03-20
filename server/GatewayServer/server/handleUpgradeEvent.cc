#include "handleUpgradeEvent.h"
#include "httpServer/HttpRequest.h"
#include "base/base64.h"
#include "websocketConn.h"
#include "GatewayPubSubManager.h"
#include "producer.h"
#include "GatewayServer.h"

#include "../../base/JsonView.h"

#include <openssl/sha.h>
#include <jwt.h>

std::string AnalysisCookie(const HttpRequest& req) {
    if(!req.headers().contains("Cookie")) return "";
    std::string cookieFields = req.getHeader("Cookie");

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
    std::string cli_websocket_key = req.getHeader("Sec-WebSocket-Key");
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

    if(req.getHeader("Upgrade") == "websocket") {
        std::string cookie = AnalysisCookie(req);

        jwt* decoded = nullptr;

        if(jwt_decode(&decoded, cookie.c_str(), (unsigned char*)GatewayServer::public_key, 
        strlen(GatewayServer::public_key)) != 0) {
            sendbadResponse(conn);

            return;
        }

        conn->send(HandleUpgradeResponse(req));
        int32_t userid = jwt_get_grant_int(decoded, "userid");
        const char* uname_ptr = jwt_get_grant(decoded, "username");
        std::string username = uname_ptr ? uname_ptr : "";

        jwt_free(decoded);

        if(username.empty()) return;

        auto wsContextPtr = std::make_shared<WebsocketConn>(conn);
        wsContextPtr->setUserid(userid);
        wsContextPtr->setUsername(username);

        client->rpcclearCursorsAsync(userid, [] () { return; });

        auto roomlist = GatewayPubSubManager::UserRoomLRU_.get(userid);

        if(roomlist) {
            EventLoop* loop = wsContextPtr->getLoop();

            loop->runInLoop([wsContextPtr, roomlist = std::move(roomlist)] () {
                for(const auto& roomid : roomlist.value()) {
                    bool needSubscribeToRedisPub = false;

                    auto& conns = LocalWebsockConnRoomhash[roomid];
                    if(conns.empty()) needSubscribeToRedisPub = true;

                    LocalWebsockConnRoomhash[roomid].insert(wsContextPtr);
                    if(needSubscribeToRedisPub) GatewayPubSubManager::SubscribeRoomSafe(roomid);

                    wsContextPtr->addRoom(roomid);
                }
            });

        } else {

            client->rpcGetUserRoomListAsync(userid, [userid, wsContextPtr] (std::vector<std::string> roomlist) {

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

                        LocalWebsockConnRoomhash[roomid].insert(wsContextPtr);
                        if(needSubscribeToRedisPub) GatewayPubSubManager::SubscribeRoomSafe(roomid);

                        wsContextPtr->addRoom(roomid);
                    }
                });
#endif
    
            });
        }

        client->rpcinitialPullMessageAsync(wsContextPtr->userid(), wsContextPtr->username(), 10, 
        [wsContextPtr] (std::string msg) { wsContextPtr->send(msg); });

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

                std::unordered_set<std::string> myRooms = wsContextPtr->getjoinedRooms();
                for(const auto& roomid : myRooms) {
                    auto it = LocalWebsockConnRoomhash.find(roomid);

                    if(it != LocalWebsockConnRoomhash.end()) {
                        it->second.erase(wsContextPtr);

                        if(it->second.empty()) {
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

            for(auto& message : messageList) {

                JsonDoc root;

                if(!root.parse(message.c_str(), message.size())) continue;

                std::string messageType = root.root()["type"].asString();

                if(messageType == "ClientMessage") {

                    std::string roomid = root.root()["payload"]["roomId"].asString();
                    std::string clientMessageId = root.root()["clientMessageId"].asString();

                    KafkaDeliveryContext* ctx = new KafkaDeliveryContext{conn, clientMessageId};

                    producer->produce("chat_room_messages", message.data(), message.size(), roomid, roomid.size(), ctx, 
                    wsContextPtr->userid(), wsContextPtr->username());

                } else if(messageType == "RequestRoomHistory" || messageType == "PullMissingMessages") {
                    client->rpcCilentMessageAsync(message, wsContextPtr->userid(), wsContextPtr->username(), 
                    [wsContextPtr] (std::string msg) {
                        wsContextPtr->send(msg);
                    });

                } else { return; }
            }
        });

    }
}
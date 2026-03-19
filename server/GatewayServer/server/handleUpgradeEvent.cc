#include "handleUpgradeEvent.h"
#include "httpServer/HttpRequest.h"
#include "base/base64.h"
#include "websocketConn.h"
#include "GatewayPubSubManager.h"
#include "producer.h"
#include "GatewayServer.h"

#include <openssl/sha.h>
#include <jwt.h>
#include <jsoncpp/json/json.h>

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

        client->rpcGetUserRoomListAsync(userid, [wsContextPtr] (std::vector<std::string> roomlist) {
            for(const auto& roomid : roomlist) {
                // 加了mutex
                GatewayPubSubManager::SubscribeRoomSafe(roomid);

                std::lock_guard<std::mutex> lock(GatewayPubSubManager::WebsockConnRoomhashMutex);
                GatewayPubSubManager::WebsockConnRoomhash[roomid].insert(wsContextPtr);
            }
        });

        client->rpcinitialPullMessageAsync(wsContextPtr->userid(), wsContextPtr->username(), 10, 
        [wsContextPtr] (std::string msg) { wsContextPtr->send(msg); });

        wsContextPtr->setWebconnCloseCallback([wsContextPtr] () {
            if(!wsContextPtr->connected()) return;

            int32_t userid = wsContextPtr->userid();

            {
                std::lock_guard<std::mutex> lock(GatewayPubSubManager::WebsockConnhashMutex);
                if(GatewayPubSubManager::WebsockConnhash.contains(userid) && 
                    GatewayPubSubManager::WebsockConnhash[userid] == wsContextPtr) {
                    GatewayPubSubManager::WebsockConnhash.erase(userid);
                }
                // 还需要去redis删除对应的路由表(分布式)
            }

            {
                std::lock_guard<std::mutex> lock(GatewayPubSubManager::WebsockConnRoomhashMutex);
                for(auto it = GatewayPubSubManager::WebsockConnRoomhash.begin(); 
                    it != GatewayPubSubManager::WebsockConnRoomhash.end(); ++it) {
                    
                    std::unordered_set<WebsocketConnPtr>& conns = it->second;
                    if(conns.find(wsContextPtr) != conns.end()) conns.erase(wsContextPtr);
                }
            }

        });

        conn->setContext(wsContextPtr);

        {
            std::lock_guard<std::mutex> lock(GatewayPubSubManager::WebsockConnhashMutex);
            GatewayPubSubManager::WebsockConnhash[userid] = wsContextPtr;
        }

        conn->setMessageCallback([producer, client] (const TcpConnectionPtr& conn, muduo::net::Buffer* buffer, muduo::Timestamp) {
            if(conn->disconnected()) return;
            WebsocketConnPtr wsContextPtr = *(std::any_cast<WebsocketConnPtr>(conn->getMutableContext()));

            std::vector<std::string> messageList = wsContextPtr->onRead(conn, buffer);

            if(messageList.empty()) return;

            for(auto& message : messageList) {
                Json::Value root;
                Json::Reader reader;

                if(!reader.parse(message, root)) continue;

                std::string messageType = root["type"].asString();

                if(messageType == "ClientMessage") {

                    std::string roomid = root["payload"]["roomId"].asString();
                    std::string clientMessageId = root["clientMessageId"].asString();

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
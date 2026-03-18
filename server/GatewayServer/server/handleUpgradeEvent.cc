#include "handleUpgradeEvent.h"
#include "httpServer/HttpRequest.h"
#include "base/base64.h"
#include "websocketConn.h"
#include "GatewayPubSubManager.h"
#include "producer.h"

#include <openssl/sha.h>

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

void handleUpgradeEvent(const TcpConnectionPtr& conn, const HttpRequest& req, const grpcClientPtr& client,
    const kafkaProducerPtr& producer) {

    if(conn->disconnected()) return;

    if(req.getHeader("Upgrade") == "websocket") {
        std::string cookie = AnalysisCookie(req);
        std::string email;
        std::string username;
        int32_t userid{0};
        int auth_res{0};

        client->rpcVerify(cookie, userid, username, auth_res);

        if(cookie.empty() || auth_res < 0) { // 校验失败
            std::string rejectRes = "HTTP/1.1 401 Unauthorized\r\n"
                                    "Connection: close\r\n"
                                    "Content-Length: 0\r\n\r\n";
            conn->send(rejectRes);
            conn->shutdown();

            return;
        }

        std::string response = HandleUpgradeResponse(req);
        conn->send(response);

        auto wsContextPtr = std::make_shared<WebsocketConn>(conn);
        wsContextPtr->setUserid(userid);
        wsContextPtr->setUsername(username);
        wsContextPtr->setgrpcClientPtr(client);

        client->rpcclearCursors(userid);

        std::vector<std::string> roomlist = client->rpcGetUserRoomList(userid);

        for(const auto& roomid : roomlist) {
            // 加了mutex
            GatewayPubSubManager::SubscribeRoomSafe(roomid);

            std::lock_guard<std::mutex> lock(GatewayPubSubManager::WebsockConnRoomhashMutex);
            GatewayPubSubManager::WebsockConnRoomhash[roomid].insert(wsContextPtr);
        }

        std::string helloMessage = client->rpcinitialPullMessage(userid, username, 10);

        wsContextPtr->send(helloMessage);

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

        conn->setMessageCallback([producer] (const TcpConnectionPtr& conn, muduo::net::Buffer* buffer, muduo::Timestamp) {
            if(conn->disconnected()) return;
            WebsocketConnPtr* wsContextPtr = std::any_cast<WebsocketConnPtr>(conn->getMutableContext());

            std::string data = (*wsContextPtr)->onRead(conn, buffer);

            if(!data.empty()) {
                Json::Value root;
                Json::Reader reader;

                if(reader.parse(data, root)) {
                    std::string roomid = root["payload"]["roomId"].asString();
                    std::string clientMessageId = root["clientMessageId"].asString();

                    KafkaDeliveryContext* ctx = new KafkaDeliveryContext{conn, clientMessageId};

                    producer->produce("chat_room_messages", data.data(), data.size(), roomid, roomid.size(), ctx, 
                    (*wsContextPtr)->userid(), (*wsContextPtr)->username());
                }
            }

        });

    }
}
#pragma once

#include <string>
#include <cstdint>
#include <vector>

struct Room {
    std::string room_id_;
    std::string room_name_;
    uint32_t creator_id_;
    std::string history_last_message_id_;
    std::string creator_time_;
    std::string update_time_;
};

struct User {
    int32_t userid_;
    std::string username_;
};

struct Message {
    // Message ID
    std::string id;

    // The actual content of the message
    std::string content;

    // UTC timestamp when the server received the message
    uint64_t timestamp; //直接存储秒的单位

    // ID of the user that sent the message
    std::int64_t user_id{};
    std::string username;
};


struct MessageBatch
{
    // The messages in the batch
    std::vector<Message> messages;

    // true if there are more messages that could be loaded
    bool has_more{};
};

std::string buildWebSocketFrame(const std::string& payload, uint8_t opcode = 0x01);

/*

{
  "type": "ClientMessage",
  "payload": {
    "clientMessageId": "msg_1710000000000_a1b2c3d",
    "roomId": "beast",
    "messages": [
      {
        "content": "这是小鸭子发送的消息",
        "msgType": "text"
      }
    ]
  }
}

{
  "type": "PullMissingMessages",
  "payload": {
    "roomId": "beast",
    "missingMessageIds": [11, 12]
  }
}

{
    "type": "ServerMessage",
    "payload": {
        "roomId": "beast",
        "message": [
            {
                "clientMessageId": ...,
                "serverMessageId": 122,
                "content": "hello world",
                "user": {
                    "id": 5,
                    "username": "m0NESY"
                },
                "timestamp": 1772972215
            },
            {
                "clientMessageId": ...,
                "serverMessageId": 123,
                "content": "batch message 2",
                "user": {
                    "id": 5,
                    "username": "m0NESY"
                },
                "timestamp": 1772972216
            }
        ]
    }
}

{
    "type": "RequestRoomHistory",
    "payload": {
        count: 20,
        firstMessageId: "63-0",
        roomId: "f3909e6e-1bc4-11f1-a7fa-000c29dfa7f1"
    }
}

{
    "type": "RequestMessage",
    "payload": {
        "roomId": "beast",
        "roomname": "coroutine",
        "hasMoreMessages": true,
        "message": [
            {
                "id": "uuid-123",
                "content": "hello world",
                "user": {
                    "id": 5,
                    "username": "m0NESY"
                },
                "timestamp": 1772972215
            },
            {
                "id": "uuid-124",
                "content": "batch message 2",
                "user": {
                    "id": 5,
                    "username": "m0NESY"
                },
                "timestamp": 1772972216
            }
        ]
    }
}

{
  "type": "MessageAck",
  "payload": {
    "clientMessageId": "msg_1710000000000_a1b2c3d",
    "status": "SUCCESS"
  }
}

*/


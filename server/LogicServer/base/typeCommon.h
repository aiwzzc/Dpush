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

struct RoomDataCache {
    std::string room_id;
    std::string room_name;
    MessageBatch msgs;
};

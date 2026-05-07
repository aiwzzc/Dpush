#include "types.h"

std::string buildWebSocketFrame(const std::string& payload, uint8_t opcode) {
    
    size_t payload_length = payload.size();

    size_t header_len = 2;
    if (payload_length >= 126 && payload_length <= 65535) header_len = 4;
    else if (payload_length > 65535) header_len = 10;

    std::string frame;
    frame.reserve(header_len + payload_length);

    frame.push_back(0x80 | (opcode & 0x0F));
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

    frame.append(payload);

    return frame;
}
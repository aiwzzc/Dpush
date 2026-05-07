#ifndef BASE_BASE64_H
#define BASE_BASE64_H

#include <vector> 
#include <string>
#include <cstdint>
 
std::string base64_encode(const std::vector<uint8_t>& data);
std::string base64_encode(const uint8_t* data, int size);

#endif


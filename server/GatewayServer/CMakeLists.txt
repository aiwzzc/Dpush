cmake_minimum_required(VERSION 3.20)
project(GatewayServer)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(muduo/net)
add_subdirectory(muduo/base)
add_subdirectory(server/httpServer)
add_subdirectory(server)
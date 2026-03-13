cmake_minimum_required(VERSION 3.20)
project(GatewayServer)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(muduo/net)
add_subdirectory(muduo/http)
add_subdirectory(server)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Compile Debug")
    # 针对 Debug 构建类型，添加一些常见的调试相关编译选项，比如开启调试符号等
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
else()
    message("Compile Release")
    # 针对 Release 构建类型，添加优化相关编译选项，例如开启一定级别的优化
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
endif()
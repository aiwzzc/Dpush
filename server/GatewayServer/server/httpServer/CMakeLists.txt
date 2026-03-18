aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} HttpServer)

add_library(muduo_httpServer ${HttpServer})

# 1. 添加 include 路径
target_include_directories(muduo_httpServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 2. 链接依赖库
target_link_libraries(muduo_httpServer PUBLIC muduo_net)
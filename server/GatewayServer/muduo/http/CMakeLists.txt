set(CMAKE_CXX_STANDARD 17)
include_directories(${CMAKE_SOURCE_DIR})

add_library(muduo_http
    HttpServer.cc
    HttpContext.cc
    HttpRequest.cc
    HttpResponse.cc
)

target_include_directories(muduo_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(muduo_http PUBLIC muduo_net)
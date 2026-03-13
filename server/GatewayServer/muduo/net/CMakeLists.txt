add_library(muduo_net
    Acceptor.cc
    Channel.cc
    EventLoop.cc
    EventLoopThreadPool.cc
    Poller.cc
    TcpConnection.cc
    TcpServer.cc
)

target_include_directories(muduo_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
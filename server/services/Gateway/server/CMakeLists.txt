aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JEMALLOC jemalloc)

set(PROTO_SRC ${CMAKE_SOURCE_DIR}/proto/auth.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/auth.pb.cc 
${CMAKE_SOURCE_DIR}/proto/logic.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/logic.pb.cc 
${CMAKE_SOURCE_DIR}/proto/room.grpc.pb.cc
${CMAKE_SOURCE_DIR}/proto/room.pb.cc 
${CMAKE_SOURCE_DIR}/proto/gateway.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/gateway.pb.cc
)

add_executable(GatewayServer ${SRC})

target_include_directories(GatewayServer PUBLIC 
${CMAKE_SOURCE_DIR}/libs 
${CMAKE_BINARY_DIR}/proto 
${CMAKE_SOURCE_DIR}/libs/tools 
${CMAKE_SOURCE_DIR}/libs/tools/rapidyaml 
${CMAKE_SOURCE_DIR}/libs/utils 
${CMAKE_CURRENT_SOURCE_DIR}/httpServer 
${CMAKE_BINARY_DIR}/fbs
)

set(GRPC_LIBS
    grpc++
    grpc
    gpr
    protobuf 
    absl_synchronization 
    absl_base
)

target_link_libraries(GatewayServer 
PRIVATE
    muduo_net 
    push_http 
    push_crypto 
    crypto 

    # ${GRPC_LIBS} 
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads

    redis++ 
    hiredis 
    rdkafka++ 
    rdkafka 
    uuid 
    jwt 
    uring 
    push_utils 
    push_yyjson 
    etcd-cpp-api 
    cpprest 
    proto_gen  
    ryml 
    ${JEMALLOC_LIBRARIES}
)


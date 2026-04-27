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

add_subdirectory(httpServer)

add_executable(GatewayServer ${SRC} ${PROTO_SRC})

target_include_directories(GatewayServer PUBLIC 
${CMAKE_SOURCE_DIR}/libs 
${CMAKE_SOURCE_DIR}/proto 
${CMAKE_SOURCE_DIR}/libs/tools 
${CMAKE_SOURCE_DIR}/libs/utils 
${CMAKE_CURRENT_SOURCE_DIR}/httpServer 
${CMAKE_SOURCE_DIR}/flatbuffers)

target_link_libraries(GatewayServer 
PRIVATE
    muduo_net 
    push_http 
    push_crypto 
    crypto 
    grpc++ 
    grpc 
    gpr 
    protobuf 
    absl_synchronization 
    redis++ 
    hiredis 
    rdkafka++ 
    rdkafka 
    uuid 
    jwt 
    uring 
    push_utils 
    push_yyjson 
    ${JEMALLOC_LIBRARIES}
)


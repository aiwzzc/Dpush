set(CMAKE_CXX_STANDARD 20)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)

set(PROTO_SRC 
${CMAKE_SOURCE_DIR}/proto/room.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/room.pb.cc 
${CMAKE_SOURCE_DIR}/proto/logic.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/logic.pb.cc 
${CMAKE_SOURCE_DIR}/proto/gateway.grpc.pb.cc 
${CMAKE_SOURCE_DIR}/proto/gateway.pb.cc
)

add_executable(LogicServer ${SRC})

target_include_directories(LogicServer PUBLIC 
${CMAKE_BINARY_DIR}/proto 
${CMAKE_SOURCE_DIR}/libs 
${CMAKE_SOURCE_DIR}/libs/concurrency 
${CMAKE_SOURCE_DIR}/libs/utils 
${CMAKE_SOURCE_DIR}/libs/tools 
${CMAKE_BINARY_DIR}/fbs 
)

find_package(utf8_range REQUIRED)

set(GRPC_LIBS
    grpc++
    grpc
    gpr
    protobuf 
    absl_synchronization 
    absl_base
)

target_link_libraries(LogicServer 
PRIVATE
    ${GRPC_LIBS} 
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads

    absl::synchronization
    absl::base
    absl::strings
    utf8_range::utf8_range

    redis++ 
    hiredis 
    push_crypto 
    mysqlcppconn
    push_mysqlpool
    rdkafka++ 
    rdkafka 
    push_utils 
    proto_gen
)
